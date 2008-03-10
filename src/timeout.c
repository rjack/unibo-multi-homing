#include "h/types.h"
#include "h/util.h"
#include "h/channel.h"
#include "h/crono.h"
#include "h/segment.h"
#include "h/timeout.h"

#include <assert.h>
#include <config.h>
#include <stdio.h>
#include <unistd.h>

#define     TYPE     timeout_t
#define     NEXT     to_next
#define     PREV     to_prev
#define     EMPTYQ   NULL
#include "src/queue_template"


/*******************************************************************************
			  Macro e definizioni locali
*******************************************************************************/

#define     HUGE_TIMEOUT     1000000000

#define     VALID_CLASS(cn)                             \
	((cn) == TOACK || (cn) == TOACT || (cn) == TONAK)


/*******************************************************************************
			       Variabili locali
*******************************************************************************/

/* Code di timeout.
 * Per ogni coda l'ordine dei timeout e' indifferente. */
static timeout_t *tqueue[TMOUTS];

/* Intervallo spedizione ACK. */
static timeout_t ack_timeout;

/* Controllo paranoia. */
static bool init_done = FALSE;


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static void ack_handler (int seq);
static void nak_handler (int seq);
static double timeout_check (timeout_t *to);


/*******************************************************************************
				   Funzioni
*******************************************************************************/


void
add_timeout (timeout_t *to, int class)
{
	/* Aggiunge to alla lista dei timeout di classe class, in modo che
	 * venga gestito da check_timeouts.
	 * XXX se to e' oneshot il chiamante non deve piu' usare il puntatore
	 * XXX a to, perche' non ha modo di sapere con sicurezza quando verra'
	 * XXX deallocato senza rischiare un segfault. */

	assert (to != NULL);
	assert (VALID_CLASS (class));
	assert (init_done);
	assert (!qcontains (tqueue[class], to));

	qenqueue (&tqueue[class], to);
}


void
add_nak_timeout (seq_t seq)
{
	timeout_t *to;

	assert (init_done);

	/* Crea un timeout oneshot per attesa spedizione nak. */
	to = timeout_create (TONAK_VAL, nak_handler, seq, TRUE);
	timeout_reset (to);
	add_timeout (to, TONAK);
}


double
check_timeouts (void)
{
	/* Chiama timeout_check su tutti i timeout.
	 * Ritorna il valore del timeout piu' prossimo a scadere. */

	int i;
	double left;
	double min = HUGE_TIMEOUT;
	timeout_t *cur;
	timeout_t *nxt;

	assert (init_done);

	for (i = 0; i < TMOUTS; i++) {
		cur = getHead (tqueue[i]);
		while (!isEmpty (tqueue[i]) && cur != NULL) {
			if (getNext (cur) == getHead (tqueue[i]))
				nxt = NULL;
			else
				nxt = getNext (cur);
			left = timeout_check (cur);
			if (left > 0)
				min = MIN (min, left);
			else if (cur->to_oneshot == TRUE) {
				del_timeout (cur, i);
				timeout_destroy (cur);
			}
			cur = nxt;
		}
	}

	return (min == HUGE_TIMEOUT ? 0 : min);
}


void
del_timeout (timeout_t *to, int class)
{
	/* Rimuove to dalla lista di classe class. */

	assert (to != NULL);
	assert (class < TMOUTS);
	assert (init_done);

	qremove (&tqueue[class], to);
}


void
del_nak_timeout (seq_t seq)
{
	timeout_t *nakto;

	assert (init_done);

	nakto = get_timeout (TONAK, seq);

	if (nakto != NULL) {
		del_timeout (nakto, TONAK);
		timeout_destroy (nakto);
	}
}


timeout_t *
get_timeout (int class, int id)
{
	timeout_t *cur;
	timeout_t *head;

	assert (VALID_CLASS (class));
	assert (init_done);

	head = getHead (tqueue[class]);
	if (head == NULL)
		return NULL;

	switch (class) {
	case TOACK :
		return head;

	case TOACT :
	case TONAK :
		cur = head;
		while (cur->to_trigger_arg != id
		       && (cur = getNext (cur)) != head)
			;
		if (cur->to_trigger_arg == id)
			return cur;
		return NULL;

	default :
		assert (FALSE);
		return NULL;
	}
}


void
init_timeout_module (void)
{
	/* Inizializza le code delle classi dei timeout e imposta il timeout
	 * degli ack. */

	int i;

	assert (!init_done);

	for (i = 0; i < TMOUTS; i++)
		tqueue[i] = newQueue ();

	timeout_init (&ack_timeout, TOACK_VAL, ack_handler, 0, FALSE);
	timeout_reset (&ack_timeout);
	add_timeout (&ack_timeout, TOACK);

	init_done = TRUE;
}


timeout_t *
timeout_create
(double maxval, timeout_handler_t trigger, int trigger_arg, bool oneshot)
{
	/* Crea e inizializza un timeout, restituendone il puntatore.
	 * Il timeout puo' essere attivato con timeout_reset. */

	timeout_t *newto;

	assert (maxval > 0);
	assert (trigger != NULL);
	assert (BOOL_VALUE (oneshot));

	newto = xmalloc (sizeof (timeout_t));
	timeout_init (newto, maxval, trigger, trigger_arg, oneshot);

	return newto;
}


void
timeout_destroy (timeout_t *to)
{
	/* Dealloca le strutture dati associate a to. */

	assert (to != NULL);
	xfree (to);
}


void
timeout_init (timeout_t *to, double maxval, timeout_handler_t trigger,
              int trigger_arg, bool oneshot)
{
	/* Inizializza il timeout con i valori dati.
	 * Il timeout puo' essere attivato con timeout_reset. */

	assert (to != NULL);
	assert (maxval > 0);
	assert (trigger != NULL);
	assert (BOOL_VALUE (oneshot));

	to->to_maxval = maxval;
	to->to_trigger = trigger;
	to->to_trigger_arg = trigger_arg;
	to->to_oneshot = oneshot;
}


void
timeout_reset (timeout_t *to)
{
	/* Reinizilizza la durata del timeout facendo ripartire il cronometro
	 * associato. */

	assert (to != NULL);
	crono_start (&to->to_crono);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static void
ack_handler (int seq)
{
	seq_t ack;

	if (ok_to_send_ack (&ack)) {
		struct segwrap *swack;
		swack = segwrap_ack_create (ack);
		urgent_add (swack);
	}
}


static void
nak_handler (int seq)
{
	struct segwrap *nak;

	nak = segwrap_nak_create (seq);
	urgent_add (nak);
}


static double
timeout_check (timeout_t *to)
{
	/* Controlla il tempo rimasto in to e, se scaduto, esegue il trigger
	 * associato e lo fa ripartire.
	 * Ritorna il tempo rimasto in to. */

	double left;

	assert (to != NULL);

	left = to->to_maxval - crono_measure (&to->to_crono);
	if (left <= 0) {
		timeout_reset (to);
		to->to_trigger (to->to_trigger_arg);
	}
	return left;
}
