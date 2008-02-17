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

/* Invarianti di una rqueue di spedizione:
 * - rq_data contiene la prima parte (al massimo tutta) della coda rq_sgmt
 * - rq_nbytes indica sempre quanti byte mancano per completare la spedizione
 *   del segmento alla testa della rq_sgmt. */

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
	assert (sw->sw_seglen <= cqueue_get_aval (rq->rq_data));

	/* Inizializza rq_nbytes se la coda segmenti uscenti e' vuota. */
	if (isEmpty (rq->rq_sgmt)) {
		assert (cqueue_get_used (rq->rq_data) == 0);
		assert (rq->rq_nbytes == 0);
		rq->rq_nbytes = sw->sw_seglen;
	}
	/* Copia dati nel buffer circolare di spedizione. */
	err = cqueue_add (rq->rq_data, sw->sw_seg, sw->sw_seglen);
	assert (!err);
	/* Accoda sewrap nei segmenti uscenti. */
	qenqueue (&rq->rq_sgmt, sw);

	return 0;
}


void
rqueue_add_nak (rqueue_t *rq, seq_t nakseq)
{
	/* Aggiunge un NAK in testa alla coda dei segmenti uscenti, oppure al
	 * secondo posto se c'e' un segmento parzialmente spedito da finire di
	 * inviare.
	 * XXX rq deve avere abbastanza spazio per contenere il NAK. */

	int err;
	struct segwrap *nak;
	struct segwrap *head;

	assert (rq != NULL);

	nak = segwrap_nak_create (nakseq);
	assert (nak->sw_seglen <= cqueue_get_aval (rq->rq_data));

	head = getHead (rq->rq_sgmt);
	assert (head == NULL || rq->rq_nbytes <= head->sw_seglen);

	/* Inserimento NAK: 3 casi. */
	/* FIXME una volta che funzia per certo, collassare i primi due. */
	/* 1. Coda vuota, semplice accodamento. */
	if (isEmpty (rq->rq_sgmt)) {
		assert (cqueue_get_used (rq->rq_data) == 0);
		assert (rq->rq_nbytes == 0);

		err = cqueue_add (rq->rq_data, nak->sw_seg, nak->sw_seglen);
		assert (!err);
		qenqueue (&rq->rq_sgmt, nak);

		rq->rq_nbytes = nak->sw_seglen;
	}
	/* 2. Primo segmento non ancora spedito, aggiunta in testa. */
	else if (head->sw_seglen == rq->rq_nbytes) {
		assert (cqueue_get_used (rq->rq_data) > 0);
		assert (rq->rq_nbytes > 0);

		err = cqueue_push (rq->rq_data, nak->sw_seg, nak->sw_seglen);
		assert (!err);
		qpush (&rq->rq_sgmt, nak);

		rq->rq_nbytes = nak->sw_seglen;
	}
	/* 3. Altrimenti inserimento subito dopo il primo segmento. */
	else {
		size_t fresh;

		assert (cqueue_get_used (rq->rq_data) > 0);
		assert (rq->rq_nbytes > 0);

		/* Rimozione primo segmento e scarto dei suoi byte rimasti. */
		head = qdequeue (&rq->rq_sgmt);
		cqueue_drop_head (rq->rq_data, rq->rq_nbytes);

		/* Inserimento NAK. */
		err = cqueue_push (rq->rq_data, nak->sw_seg, nak->sw_seglen);
		assert (!err);
		qpush (&rq->rq_sgmt, nak);

		/* Reinserimento primo segmento a partire dai byte non ancora
		 * spediti. */
		fresh = head->sw_seglen - rq->rq_nbytes;
		err = cqueue_push (rq->rq_data,
				&head->sw_seg[fresh],
				rq->rq_nbytes);
		assert (!err);
		qpush (&rq->rq_sgmt, head);
	}
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


struct segwrap *
rqueue_remove (rqueue_t *rq)
{
	struct segwrap *rmvd;
	struct segwrap *head;

	assert (rq != NULL);
	assert (rq->rq_sgmt != NULL);

	rmvd = qdequeue (&rq->rq_sgmt);
	if (rmvd != NULL) {
		assert (rq->rq_nbytes > 0);
		cqueue_drop_head (rq->rq_data, rq->rq_nbytes);
		head = getHead (rq->rq_sgmt);
		if (head != NULL) {
			rq->rq_nbytes = head->sw_seglen;
		}
	}

	return rmvd;
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
		 * registra tra quelli spediti. */
		if (rq->rq_nbytes == 0) {
			struct segwrap *head;

			head = qdequeue (&rq->rq_sgmt);
			assert (head != NULL);

			/* Scartare i NAK e' responsabilita' di
			 * handle_sent_segment. */
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
