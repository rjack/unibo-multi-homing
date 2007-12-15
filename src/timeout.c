#include "h/types.h"
#include "h/util.h"
#include "h/crono.h"

#include <assert.h>
#include <unistd.h>

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
static timeout_t *head[CLASSNO];
static timeout_t **tail[CLASSNO];


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static double timeout_check (timeout_t *to);
static void timeout_reset (timeout_t *to);


/*******************************************************************************
				   Funzioni
*******************************************************************************/

void
init_timeout_module (void)
{
	/* Da chiamare prima di ogni altra funzione del modulo;
	 * "static" assicura che abbia effetto una volta sola. */

	static int i = 0;
	while (i < CLASSNO) {
		head[i] = NULL;
		tail[i] = &head[i];
		i++;
	}
}


void
add_timeout (timeout_t *to, timeout_class class)
{
	assert (to != NULL);

	to->to_next = NULL;
	*(tail[class]) = to;
	tail[class] = &(to->to_next);
}


double
check_timeouts (void)
{
	/* Chiama timeout_check su tutti i timeout.
	 * Ritorna il valore del timeout piu' prossimo a scadere. */

	int i;
	double left;
	double min = HUGE_TIMEOUT;
	timeout_t *to;

	for (i = 0; i < CLASSNO; i++) {
		for (to = head[i]; to != NULL; to = to->to_next) {
			left = timeout_check (to);
			if (left > 0) {
				min = MIN (min, left);
			} else if (to->to_oneshot == TRUE) {
				/* TODO
				 * timeout remove (to);
				 * xfree (to) */
			}
		}
	}
	return (min == HUGE_TIMEOUT ? 0 : min);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

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
		to->to_trigger (to->to_trigger_args);
		timeout_reset (to);
	}
	return left;
}


static void
timeout_reset (timeout_t *to)
{
	assert (to != NULL);
	crono_start (&to->to_crono);
}
