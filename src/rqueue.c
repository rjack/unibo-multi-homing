#include "h/rqueue.h"
#include "h/cqueue.h"
#include "h/channel.h"
#include "h/segment.h"
#include "h/types.h"
#include "h/util.h"

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
static bool is_first_partially_sent (rqueue_t *rq);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/


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
	assert (isEmpty (rq->rq_sgmt)
	        || is_first_partially_sent (rq)
	        || segwrap_urgcmp (rq->rq_sgmt, sw) < 0);

	if (rqueue_get_used (rq) == 0)
		rq->rq_nbytes = sw->sw_seglen;

	qenqueue (&rq->rq_sgmt, sw);
	err = cqueue_add (rq->rq_data, sw->sw_seg, sw->sw_seglen);
	assert (!err);

	return 0;
}


bool
rqueue_can_read (rqueue_t *rq)
{
	assert (rq != NULL);
	return cqueue_can_read (rq->rq_data);
}


bool
rqueue_can_write (rqueue_t *rq)
{
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

	struct segwrap *head;
	struct segwrap *rmvdq;

	assert (rq != NULL);

	head = getHead (rq->rq_sgmt);

	if (head == NULL) {
		assert (rqueue_get_used (rq) == 0);
		return newQueue ();
	}
	assert (rqueue_get_used (rq) > 0);

	rmvdq = rq->rq_sgmt;
	rq->rq_sgmt = newQueue ();
	head = getHead (rmvdq);
	assert (rq->rq_nbytes > 0);
	if (rq->rq_nbytes < head->sw_seglen)
		qenqueue (&rq->rq_sgmt, qdequeue (&rmvdq));
	else
		rq->rq_nbytes = 0;

	consolidate (rq);
	return rmvdq;
}


void
rqueue_destroy (rqueue_t *rq)
{
	assert (rq != NULL);
	assert (isEmpty (rq->rq_sgmt));

	cqueue_destroy (rq->rq_data);
	xfree (rq);
}


size_t
rqueue_get_aval (rqueue_t *rq)
{
	size_t cqaval;

	assert (rq != NULL);
	cqaval = cqueue_get_aval (rq->rq_data);

#ifndef NDEBUG
	if (cqaval == rq->rq_data->cq_len) {
		assert (isEmpty (rq->rq_sgmt));
		assert (rq->rq_nbytes == 0);
	} else {
		struct segwrap *head;
		head = getHead (rq->rq_sgmt);
		assert (!isEmpty (rq->rq_sgmt));
		assert (rq->rq_nbytes > 0);
		assert (head->sw_seglen >= rq->rq_nbytes);
	}
#endif /* NDEBUG */

	return cqaval;
}


size_t
rqueue_get_used (rqueue_t *rq)
{
	size_t cqused;

	assert (rq != NULL);

	cqused = cqueue_get_used (rq->rq_data);

#ifndef NDEBUG
	if (cqused == 0) {
		assert (isEmpty (rq->rq_sgmt));
		assert (rq->rq_nbytes == 0);
	} else {
		struct segwrap *head;
		head = getHead (rq->rq_sgmt);
		assert (!isEmpty (rq->rq_sgmt));
		assert (rq->rq_nbytes > 0);
		assert (head->sw_seglen >= rq->rq_nbytes);

	}
#endif /* NDEBUG */

	return cqused;
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
		bool full_segment;
#ifndef NDEBUG
		fprintf (stdout, "rqueue_read %d bytes\n", nread);
		fflush (stdout);
#endif

		full_segment = FALSE;
		while ((seglen = cqueue_seglen (rq->rq_data)) > 0) {
			struct segwrap *sw;
			sw = segwrap_create ();
			sw->sw_seglen = seglen;
			err = cqueue_remove (rq->rq_data, sw->sw_seg, seglen);
			assert (!err);
			handle_rcvd_segment (sw);
			full_segment = TRUE;
		}
		if (full_segment)
			channel_activity_notice (get_cd_from (rq, ELRQUEUE));
}

	errno = errno_s;
	return nread;
}


void
rqueue_rm_acked (rqueue_t *rq, struct segwrap *ack)
{
	/* Rimuove e distrugge tutti i segwrap che non devono piu' essere
	 * spediti perche' hanno il seqnum minore o uguale ad ack, tranne il
	 * primo se e' parzialmente spedito.
	 * Se necessario, consolida rq. */

	struct segwrap *head;
	struct segwrap *rmvdq;

	head = getHead (rq->rq_sgmt);
	if (head == NULL)
		return;

	/* Se il primo e' stato spedito parzialmente lo salva a parte e lo
	 * ripristina successivamente. */
	if (rq->rq_nbytes < head->sw_seglen)
		head = qdequeue (&rq->rq_sgmt);
	else
		head = NULL;

	/* Rimozione acked. */
	rmvdq = qremove_all_that (&rq->rq_sgmt, &segwrap_is_acked, ack);

	/* Ripristino primo segmento parziale. */
	if (head != NULL)
		qpush (&rq->rq_sgmt, head);

	/* Se sono stati rimossi dei segmenti, la coda va riportata in uno
	 * stato coerente. */
	if (!isEmpty (rmvdq))
		consolidate (rq);

	/* Deallocazione rimossi. */
	while (!isEmpty (rmvdq))
		segwrap_destroy (qdequeue (&rmvdq));
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
	bool full_segment;

	assert (fd >= 0);
	assert (rq != NULL);
	assert (rqueue_can_write (rq));
	assert (rq->rq_nbytes > 0);

	retval = nsent = cqueue_write (fd, rq->rq_data);
	errno_s = errno;

	full_segment = FALSE;
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
			full_segment = TRUE;

			/* Ricalcola rq_nbytes. */
			head = getHead (rq->rq_sgmt);
			if (head != NULL)
				rq->rq_nbytes = head->sw_seglen;
			else
				assert (nsent == 0);
		}
	}
	if (full_segment)
		channel_activity_notice (get_cd_from (rq, ELRQUEUE));

	errno = errno_s;
	return retval;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static void
consolidate (rqueue_t *rq)
{
	/* Riporta rq_data in uno stato coerente con rq_sgmt.
	 * Se il segmento in testa e' stato parzialmente spedito lo ignora,
	 * perche' assume che le altre funzioni non lo modifichino. */

	struct segwrap *head;
	struct segwrap *tmp;
	size_t todrop;

	assert (rq != NULL);

	todrop = cqueue_get_used (rq->rq_data);
	if (todrop == 0)
		return;

	tmp = rq->rq_sgmt;
	rq->rq_sgmt = newQueue ();
	head = getHead (tmp);
	if (head != NULL && head->sw_seglen > rq->rq_nbytes) {
		qenqueue (&rq->rq_sgmt, qdequeue (&tmp));
		todrop -= rq->rq_nbytes;
	} else
		rq->rq_nbytes = 0;

	if (todrop)
		cqueue_drop_tail (rq->rq_data, todrop);

	while (!isEmpty (tmp))
		rqueue_add (rq, qdequeue (&tmp));

}


static bool
is_first_partially_sent (rqueue_t * rq)
{
	struct segwrap *head;

	assert (rq != NULL);

	head = getHead (rq->rq_sgmt);
	if (head != NULL) {
		assert (rq->rq_nbytes > 0);
		if (rq->rq_nbytes < head->sw_seglen)
			return TRUE;
	}
	return FALSE;
}
