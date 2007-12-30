#include "h/cqueue.h"
#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <config.h>

#define     TYPE     struct segwrap
#define     NEXT     sw_next
#define     PREV     sw_prev
#define     EMPTYQ   NULL
#include "src/queue_template"


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

	qenqueue (&rq->rq_seg, sw);
	err = cqueue_add (rc->rq_data, sw->sw_seg, sw->sw_seglen);
	assert (!err);
	return err;
}


void
rqueue_add_nak (rqueue_t *rq, seq_t seq)
{
	/* TODO rqueue_add_nak */
}


bool
rqueue_can_read (void *arg)
{
	/* TODO rqueue_can_read */
	return FALSE;
}


bool
rqueue_can_write (void *arg)
{
	/* TODO rqueue_can_write */
	return FALSE;
}


rqueue_t *
rqueue_create (size_t len)
{
	rqueue_t *newrq;

	assert (len > 0);

	newrq = xmalloc (sizeof (rqueue_t));
	newrq->rq_data = rqueue_create (len);
	newrq->rq_seg = newQueue ();
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
	/* TODO rqueue_write */
	return 0;
}
