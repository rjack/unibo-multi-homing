#include "h/types.h"
#include "h/util.h"
#include "h/crono.h"
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
			      Definizioni locali
*******************************************************************************/

#define     HUGE_TIMEOUT     1000000000
#define     CLASSNO          3


/*******************************************************************************
			       Variabili locali
*******************************************************************************/

/* Teste e code delle linked list di timeout.
 * Per ogni classe l'ordine dei timeout e' indifferente. */
static timeout_t *tqueue[CLASSNO];

/* Intervallo spedizione ACK.
 * XXX per ora viene solo inizializzato da init_timeout_module, quindi non
 * XXX viene controllato da check_timeouts.
 * XXX Sara' passato ad add_timeout alla ricezione del primo segmento. */
static timeout_t ack_timeout;

/* Per controllare che le funzioni di gestione dei timeout non vengano
 * chiamate senza aver inizializzato il modulo. */
static bool init_done = FALSE;


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static double timeout_check (timeout_t *to);
static void ack_handler (void *args);


/*******************************************************************************
				   Funzioni
*******************************************************************************/


void
add_timeout (timeout_t *to, timeout_class class)
{
	/* Aggiunge to alla lista dei timeout di classe class, in modo che
	 * venga gestito da check_timeouts.
	 * XXX se to e' oneshot il chiamante non deve piu' usare il puntatore
	 * a to, perche' non ha modo di sapere con sicurezza quando verra'
	 * deallocato senza rischiare un segfault. */

	assert (to != NULL);
	assert (class < CLASSNO);
	assert (init_done);

	qenqueue (&tqueue[class], to);
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
	timeout_t *tmp;

	assert (init_done);

	for (i = 0; i < CLASSNO; i++) {
		cur = getHead (tqueue[i]);
		while (!isEmpty (tqueue[i]) && cur != NULL) {
			tmp = (cur->to_next == getHead (tqueue[i]) ?
			       NULL : cur->to_next);
			left = timeout_check (cur);
			if (left > 0) {
				min = MIN (min, left);
			} else if (cur->to_oneshot == TRUE) {
				del_timeout (cur, i);
				timeout_destroy (cur);
			}
			cur = tmp;
		}
	}

	return (min == HUGE_TIMEOUT ? 0 : min);
}


void
del_timeout (timeout_t *to, timeout_class class)
{
	/* Rimuove to dalla lista di classe class. */

	assert (to != NULL);
	assert (class < CLASSNO);
	assert (init_done);

	qremove (&tqueue[class], to);
}


void
init_timeout_module (void)
{
	/* Inizializza le code delle classi dei timeout e imposta il timeout
	 * degli ack. */

	int i;

	assert (!init_done);

	i = 0;
	while (i < CLASSNO) {
		tqueue[i] = newQueue ();
		i++;
	}

	timeout_init (&ack_timeout, ACK, ack_handler, NULL, FALSE);

	init_done = TRUE;
}


timeout_t *
timeout_create
(double maxval, timeout_handler_t trigger, void *trigger_args, bool oneshot)
{
	/* Crea e inizializza un timeout, restituendone il puntatore.
	 * Il timeout puo' essere attivato con timeout_reset. */

	timeout_t *newto;

	assert (maxval > 0);
	assert (trigger != NULL);
	assert (BOOL_VALUE (oneshot));

	newto = xmalloc (sizeof (timeout_t));
	timeout_init (newto, maxval, trigger, trigger_args, oneshot);

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
              void *trigger_args, bool oneshot)
{
	/* Inizializza il timeout con i valori dati.
	 * Il timeout puo' essere attivato con timeout_reset. */

	assert (to != NULL);
	assert (maxval > 0);
	assert (trigger != NULL);
	assert (BOOL_VALUE (oneshot));

	to->to_maxval = maxval;
	to->to_trigger = trigger;
	to->to_trigger_args = trigger_args;
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
ack_handler (void *args)
{
	/* TODO manda ack */
	fprintf (stderr, "ACK!\n");
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
		to->to_trigger (to->to_trigger_args);
	}
	return left;
}
