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

/* Invarianti di una rqueue:
 * - rq_data contiene la prima parte (al massimo tutta) della coda rq_seg
 * - rq_nbytes indica sempre quanti byte mancano per completare la spedizione
 *   del segmento alla testa della rq_seg. */

/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
rqueue_add (rqueue_t *rq, struct segwrap *sw)
{
	int err;

	assert (rq != NULL);
	assert (sw != NULL);
	assert (sw->sw_next == NULL);
	assert (sw->sw_prev == NULL);
	assert (sw->sw_seglen > 0);
	assert (!seg_is_nak (sw->sw_seg));

	if (isEmpty (rq->rq_seg)) {
		rq->rq_nbytes = sw->sw_seglen;
	}
	qenqueue (&rq->rq_seg, sw);
	err = cqueue_add (rq->rq_data, sw->sw_seg, sw->sw_seglen);
	assert (!err);
	return err;
}


void
rqueue_add_nak (rqueue_t *rq, seq_t seq)
{
	/* TODO rqueue_add_nak */
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
	newrq->rq_seg = newQueue ();
	newrq->rq_sent = newQueue ();
	newrq->rq_nbytes = 0;

	return newrq;
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


int
rqueue_read (fd_t fd, rqueue_t *rq)
{
	/* TODO rqueue_read */
	return 0;
}


struct segwrap *
rqueue_remove (rqueue_t *rq)
{
	struct segwrap *sw;

	assert (rq != NULL);
	assert (rq->rq_seg != NULL);

	sw = qdequeue (&rq->rq_seg);
	if (sw != NULL) {
		cqueue_drop (rq->rq_data, sw->sw_seglen);
	}
	return sw;
}


int
rqueue_write (fd_t fd, rqueue_t *rq)
{
	ssize_t nsent;

	assert (fd >= 0);
	assert (rq != NULL);
	assert (rqueue_can_write (rq));
	assert (rq->rq_nbytes > 0);

	nsent = cqueue_write (fd, rq->rq_data);

	while (nsent > 0) {
		size_t min = MIN (nsent, rq->rq_nbytes);
		nsent -= min;
		rq->rq_nbytes -= min;

		/* Se primo segmento spedito completamente, lo rimuove e lo
		 * accoda a rq_sent. */
		if (rq->rq_nbytes == 0) {
			struct segwrap *head;
			head = qdequeue (&rq->rq_seg);
			assert (head != NULL);
			qenqueue (&rq->rq_sent, head);
			/* Ricalcola rq_nbytes. */
			head = getHead (rq->rq_seg);
			if (head != NULL) {
				rq->rq_nbytes = head->sw_seglen;
			}
		}
	}

	if (errno == 0)
		return 0;
	return -1;
}
