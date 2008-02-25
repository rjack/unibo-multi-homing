#include "h/rqueue.h"
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


int
rqueue_add (rqueue_t *rq, struct segwrap *sw)
{
	/* Accoda sw alla coda rq e lo copia nel buffer.
	 * La coda deve risultare in ordine di urgenza e sw deve poter essere
	 * contenuto nel buffer. */

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

	rmvdq = newQueue ();
	head = getHead (rq->rq_sgmt);

	if (head == NULL) {
		assert (rqueue_get_used (rq) == 0);
		return rmvdq;
	}
	assert (rqueue_get_used (rq) > 0);

	rmvdq = rq->rq_sgmt;
	rq->rq_sgmt = newQueue ();
	head = getHead (rmvdq);
	if (head->sw_seglen < rq->rq_nbytes) {
		qenqueue (&rq->rq_sgmt, qdequeue (&rmvdq));
	} else
		rq->rq_nbytes = 0;


	consolidate (rq);
	return rmvdq;
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
	/* Riporta rq_data in uno stato coerente con rq_sgmt.
	 * Se il segmento in testa e' stato parzialmente spedito lo ignora,
	 * perche' assume che le altre funzioni non lo modifichino. */

	struct segwrap *head;
	struct segwrap *tmp;
	size_t todrop;

	assert (rq != NULL);

	if (rqueue_get_used (rq) == 0)
		return;

	todrop = cqueue_get_used (rq->rq_data);
	tmp = rq->rq_sgmt;
	rq->rq_sgmt = newQueue ();
	head = getHead (tmp);
	if (head->sw_seglen > rq->rq_nbytes) {
		qenqueue (&rq->rq_sgmt, qdequeue (&tmp));
		todrop -= rq->rq_nbytes;
	} else
		rq->rq_nbytes = 0;
	cqueue_drop_tail (rq->rq_data, todrop);

	while (!isEmpty (tmp))
		rqueue_add (rq, qdequeue (&tmp));
}
