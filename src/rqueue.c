#include "h/cqueue.h"
#include "h/segment.h"
#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <errno.h>
#include <config.h>

#define     TYPE     struct segwrap
#define     NEXT     sw_next
#define     PREV     sw_prev
#define     EMPTYQ   NULL
#include "src/queue_template"


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static void consolidate (rqueue_t *rq);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
rqueue_ack_prune (rqueue_t *rq, seq_t ack)
{
	/* Rimuove tutti i segwrap che non devono piu' essere spediti perche'
	 * hanno il seqnum minore o uguale ad ack, tranne il primo se e'
	 * parzialmente spedito.
	 * Se necessario, consolida rq. */

	/* TODO rqueue_ack_prune */
}


int
rqueue_add (rqueue_t *rq, struct segwrap *sw)
{
	/* Accoda sw alla coda rq e lo copia nel buffer.
	 * La coda deve risultare in ordine di urgenza e sw deve poter essere
	 * contenuto nel buffer. */

	int err;

	assert (rq != NULL);
	assert (sw != NULL);
	assert (sw->sw_next == NULL);
	assert (sw->sw_prev == NULL);
	assert (sw->sw_seglen > 0);
	assert (sw->sw_seglen <= cqueue_get_aval (rq->rq_data));
	assert (rq->rq_sgmt == NULL || segwrap_urgcmp (rq->rq_sgmt, sw) < 0);

	/* TODO rqueue_add */

	return 0;
}


bool
rqueue_can_read (rqueue_t *rq)
{
	assert (rq != NULL);
	return cqueue_can_read (rq->rq_data);
}


bool
rqueue_can_write (void *arg)
{
	rqueue_t *rq = (rqueue_t *)arg;

	assert (rq != NULL);
	return cqueue_can_write ((void *)rq->rq_data);
}


rqueue_t *
rqueue_create (size_t len)
{
	rqueue_t *newrq;

	assert (len > 0);

	newrq = xmalloc (sizeof (rqueue_t));
	newrq->rq_data = cqueue_create (len);
	newrq->rq_sgmt = newQueue ();
	newrq->rq_nbytes = 0;

	return newrq;
}


struct segwrap *
rqueue_cut_unsent (rqueue_t *rq)
{
	/* Ritorna una coda contenente i segmenti non ancora spediti, che
	 * vengono rimossi da rq_sgmt. Il buffer circolare viene modificato di
	 * conseguenza.
	 * Puo' ritornare una coda vuota. */

	/* TODO rqueue_cut_unsent */
}


void
rqueue_destroy (rqueue_t *rq)
{
	/* TODO rqueue_destroy */
	assert (FALSE);
}


size_t
rqueue_get_aval (rqueue_t *rq)
{
	assert (rq != NULL);
	return cqueue_get_aval (rq->rq_data);
}


size_t
rqueue_get_used (rqueue_t *rq)
{
	assert (rq != NULL);
	return cqueue_get_used (rq->rq_data);
}


size_t
rqueue_read (fd_t fd, rqueue_t *rq)
{
	/* Chiama cqueue_read su fd e rq->rq_data e gestisce i segmenti letti
	 * completamente.
	 * Ritorna esattamente il valore e l'errno di cqueue_read. */

	int errno_s;
	int err;
	size_t nread;

	assert (fd >= 0);
	assert (rq != NULL);
	assert (isEmpty (rq->rq_sgmt));
	assert (rq->rq_nbytes == 0);
	assert (rqueue_can_read (rq));

	nread = cqueue_read (fd, rq->rq_data);
	errno_s = errno;

	if (nread > 0) {
		size_t seglen;
		while ((seglen = cqueue_seglen (rq->rq_data)) > 0) {
			struct segwrap *sw;
			sw = segwrap_create ();
			err = cqueue_remove (rq->rq_data, sw->sw_seg, seglen);
			assert (!err);
			handle_rcvd_segment (sw);
		}
	}

	errno = errno_s;
	return nread;
}


size_t
rqueue_write (fd_t fd, rqueue_t *rq)
{
	/* Chiama cqueue_write su fd e rq->rq_data e gestisce di conseguenza
	 * la coda dei segwrap uscenti.
	 * Ritorna esattamente il valore e l'errno di cqueue_write. */

	int errno_s;
	size_t nsent;
	size_t retval;

	assert (fd >= 0);
	assert (rq != NULL);
	assert (rqueue_can_write (rq));
	assert (rq->rq_nbytes > 0);

	retval = nsent = cqueue_write (fd, rq->rq_data);
	errno_s = errno;

	while (nsent > 0) {
		size_t min;

		min = MIN (nsent, rq->rq_nbytes);
		nsent -= min;
		rq->rq_nbytes -= min;

		/* Se primo segmento spedito completamente, lo rimuove e lo
		 * gestisce. */
		if (rq->rq_nbytes == 0) {
			struct segwrap *head;

			head = qdequeue (&rq->rq_sgmt);
			assert (head != NULL);

			handle_sent_segment (head);

			/* Ricalcola rq_nbytes. */
			head = getHead (rq->rq_sgmt);
			if (head != NULL)
				rq->rq_nbytes = head->sw_seglen;
			else
				assert (nsent == 0);
		}
	}

	errno = errno_s;
	return retval;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static void
consolidate (rqueue_t *rq)
{
	/* Copia uno dopo l'altro i segmenti della coda di segrap di rq nella
	 * coda circolare; quelli che non ci stanno vengono riaccodati alla
	 * coda urgente appropriata.
	 * I segwrap devono essere gia' ordinati per urgenza. */

	/* TODO consolidate */
}
