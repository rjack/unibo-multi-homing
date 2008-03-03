#include "h/cqueue.h"
#include "h/segment.h"
#include "h/types.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>
#include <errno.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*******************************************************************************
				    Macro
*******************************************************************************/

/* Incremento e decremento circolare. */
#define     CINC(x,inc,len)     ((x) = ((x) + (inc)) % (len))
#define     CDEC(x,dec,len)     ((x) = ((x) - (dec)) % (len))

#if !HAVE_MSG_NOSIGNAL
#define     MSG_NOSIGNAL     0
#endif


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static size_t cqueue_get_aval_chunk (const cqueue_t *cq);
static size_t cqueue_get_used_chunk (const cqueue_t *cq);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
cqueue_add (cqueue_t *cq, seg_t *buf, size_t nbytes)
{
	/* Copia nbytes bytes da buf in coda a cq.
	 * Ritorna 0 se riesce, -1 se cq non ha abbastanza spazio. */

	size_t chunk_1;
	size_t chunk_2;

	assert (cq != NULL);
	assert (buf != NULL);
	assert (nbytes > 0);

	if (cqueue_get_aval (cq) >= nbytes) {
		assert (cq->cq_wrap || cq->cq_tail >= cq->cq_head);
		assert (!cq->cq_wrap || cq->cq_tail < cq->cq_head);

		chunk_1 = MIN (cqueue_get_aval_chunk (cq), nbytes);
		chunk_2 = (nbytes > chunk_1 ? nbytes - chunk_1 : 0);

		memcpy (&cq->cq_data[cq->cq_tail], buf, chunk_1);
		CINC (cq->cq_tail, chunk_1, cq->cq_len);
		buf += chunk_1;
		if (cq->cq_tail == 0) {
			assert (!cq->cq_wrap);
			cq->cq_wrap = TRUE;
		}

		if (chunk_2 > 0) {
			memcpy (&cq->cq_data[cq->cq_tail], buf, chunk_2);
			CINC (cq->cq_tail, chunk_2, cq->cq_len);
		}
		return 0;
	}
	return -1;
}


bool
cqueue_can_read (void *arg)
{
	cqueue_t *cq;

	assert (arg != NULL);

	cq = (cqueue_t *) arg;
	if (cqueue_get_aval (cq) > 0)
		return TRUE;
	return FALSE;
}


bool
cqueue_can_write (void *arg)
{
	cqueue_t *cq;

	assert (arg != NULL);

	cq = (cqueue_t *) arg;
	if (cqueue_get_used (cq) > 0)
		return TRUE;
	return FALSE;
}


cqueue_t *
cqueue_create (size_t len)
{
	cqueue_t *cq;

	assert (len > 0);

	cq = xmalloc (sizeof (cqueue_t));
	cq->cq_data = xmalloc (len * sizeof (seg_t));
	cq->cq_len = len;
	cq->cq_head = 0;
	cq->cq_tail = 0;
	cq->cq_wrap = FALSE;

	return cq;
}


void
cqueue_destroy (cqueue_t *cq)
{
	assert (cq != NULL);

	xfree (cq->cq_data);
	xfree (cq);
}


void
cqueue_drop_head (cqueue_t *cq, size_t nbytes)
{
	/* Scart nbytes bytes dalla testa di cq. */

	assert (cq != NULL);
	assert (nbytes > 0);
	assert (nbytes <= cqueue_get_used (cq));

	CINC (cq->cq_head, nbytes, cq->cq_len);
	if (cq->cq_head <= cq->cq_tail)
		cq->cq_wrap = FALSE;
}


void
cqueue_drop_tail (cqueue_t *cq, size_t nbytes)
{
	/* Scart nbytes bytes dalla coda di cq. */

	assert (cq != NULL);
	assert (nbytes > 0);
	assert (nbytes <= cqueue_get_used (cq));

	CDEC (cq->cq_tail, nbytes, cq->cq_len);
	if (cq->cq_head <= cq->cq_tail)
		cq->cq_wrap = FALSE;
}


size_t
cqueue_get_aval (cqueue_t *cq)
{
	/* Ritorna il numero di byte disponibili in cq. */

	size_t aval;

	assert (cq != NULL);

	if (cq->cq_wrap) {
		assert (cq->cq_head >= cq->cq_tail);
		aval = cq->cq_head - cq->cq_tail;
	} else {
		assert (cq->cq_head <= cq->cq_tail);
		aval = cq->cq_len - (cq->cq_tail - cq->cq_head);
	}
	return aval;
}


size_t
cqueue_get_used (cqueue_t *cq)
{
	/* Ritorna il numero di byte usati in cq. */

	assert (cq != NULL);
	return (cq->cq_len - cqueue_get_aval (cq));
}


size_t
cqueue_seglen (cqueue_t *cq)
{
	/* Ritorna la lunghezza del primo segmento contenuto se cq ha in testa
	 * un segmento completo, 0 altrimenti.
	 * XXX assume che il byte in testa sia l'inizio di un segmento e che
	 * quindi contenga il campo flags. */

	seg_t *flgptr;
	size_t used;

	assert (cq != NULL);

	used = cqueue_get_used (cq);
	if (used == 0)
		return 0;

	flgptr = &cq->cq_data[cq->cq_head];

	if (seg_is_nak (flgptr) && used >= NAKLEN) {
#ifndef NDEBUG
		fprintf (stdout, "cqueue_seglen NAK\n");
		fflush (stdout);
#endif
		return NAKLEN;
	}

	if (seg_is_ack (flgptr) && used >= ACKLEN) {
#ifndef NDEBUG
		fprintf (stdout, "cqueue_seglen ACK\n");
		fflush (stdout);
#endif
		return ACKLEN;
	}

	assert (*flgptr & PLDFLAG);

	/* Payload di lunghezza standard. */
	if (!(*flgptr & LENFLAG)) {
#ifndef NDEBUG
		fprintf (stdout, "cqueue_seglen NO LENFLAG...");
		fflush (stdout);
#endif
		if (used >= FLGLEN + SEQLEN + PLDDEFLEN) {
#ifndef NDEBUG
			fprintf (stdout, "OK\n");
			fflush (stdout);
#endif
			return (FLGLEN + SEQLEN + PLDDEFLEN);
		}
	}
	/* Payload di lunghezza non standard, bisogna accedere al campo len,
	 * se presente. */
	else {
#ifndef NDEBUG
		fprintf (stdout, "cqueue_seglen LENFLAG...");
		fflush (stdout);
#endif
		if (used > LEN) {
			int i;
			i = (cq->cq_head + LEN) % cq->cq_len;
			if (used >= FLGLEN + SEQLEN + LENLEN + cq->cq_data[i]) {
#ifndef NDEBUG
				fprintf (stdout, "OK\n");
				fflush (stdout);
#endif
				return (FLGLEN + SEQLEN + LENLEN + cq->cq_data[i]);
			}
		}
	}

#ifndef NDEBUG
	fprintf (stdout, "BHO!\n");
	fflush (stdout);
#endif
	return 0;
}


int
cqueue_push (cqueue_t *cq, seg_t *buf, size_t nbytes)
{
	/* Copia nbytes byte da buf in testa a cq. */

	size_t chunk_1;
	size_t chunk_2;

	if (cqueue_get_aval (cq) >= nbytes) {
		assert (cq->cq_wrap || cq->cq_tail >= cq->cq_head);
		assert (!cq->cq_wrap || cq->cq_tail < cq->cq_head);

		chunk_1 = cq->cq_head - (cq->cq_wrap ? cq->cq_tail : 0);
		chunk_2 = (nbytes > chunk_1 ? nbytes - chunk_1 : 0);

		if (chunk_1 > 0) {
			CDEC (cq->cq_head, chunk_1, cq->cq_len);
			memcpy (&cq->cq_data[cq->cq_head], buf, chunk_1);
			buf += chunk_1;
		}
		if (cq->cq_head + chunk_1 > cq->cq_len) {
			assert (!cq->cq_wrap);
			cq->cq_wrap = TRUE;
		}
		if (chunk_2 > 0) {
			memcpy (&cq->cq_data[cq->cq_head], buf, chunk_2);
			cq->cq_head -= chunk_2;
			assert (cq->cq_head >= 0);
			assert (cq->cq_head >= cq->cq_tail);
		}
		return 0;
	}
	return -1;
}


size_t
cqueue_read (fd_t fd, cqueue_t *cq)
{
	/* Legge piu' byte possibili da fd dentro a cq.
	 * Ritorna il numero di byte letti (0 o piu').
	 * In caso di errore imposta errno come quello di read, altrimenti a
	 * zero; se la read legge l'EOF imposta errno a EREOF. */

	ssize_t nread;
	size_t nrcvd;
	size_t chunk;

	assert (fd > 0);
	assert (cq != NULL);
	assert (cqueue_get_aval (cq) > 0);
	/* TODO assert (NONBLOCK); */

	chunk = cqueue_get_aval_chunk (cq);
	assert (chunk > 0);

	/* Questo ciclo viene eseguito al massimo due volte. */
	nrcvd = 0;
	do {
		nread = read (fd, &(cq->cq_data[cq->cq_tail]), chunk);
		if (nread > 0) {
			nrcvd += nread;
			CINC (cq->cq_tail, nread, cq->cq_len);
			if (cq->cq_tail == 0) {
				assert (!cq->cq_wrap);
				cq->cq_wrap = TRUE;
				/* Se chunk > 0 vinciamo un altro giro. */
				chunk = cqueue_get_aval_chunk (cq);
			}
		}
	} while ((nread > 0 && cq->cq_tail == 0 && chunk > 0)
	          || (nread == -1 && errno == EINTR));

	if (nread > 0)
		errno = 0;
	else if (nread == 0)
		errno = EREOF;

	return nrcvd;
}


int
cqueue_remove (cqueue_t *cq, seg_t *buf, size_t nbytes)
{
	/* Copia nbytes byte dalla testa di cq in buf rimuovendoli da cq. */

	size_t chunk_1;
	size_t chunk_2;

	assert (cq != NULL);
	assert (buf != NULL);

	if (cqueue_get_used (cq) >= nbytes) {
		assert (cq->cq_wrap || cq->cq_tail > cq->cq_head);
		assert (!cq->cq_wrap || cq->cq_tail <= cq->cq_head);

		chunk_1 = MIN (cqueue_get_used_chunk (cq), nbytes);
		chunk_2 = (nbytes > chunk_1 ? nbytes - chunk_1 : 0);

		memcpy (buf, &(cq->cq_data[cq->cq_head]), chunk_1);
		CINC (cq->cq_head, chunk_1, cq->cq_len);
		buf += chunk_1;
		if (cq->cq_head == 0) {
			assert (cq->cq_wrap);
			cq->cq_wrap = FALSE;
		}

		if (chunk_2 > 0) {
			memcpy (buf, &(cq->cq_data[cq->cq_head]), chunk_2);
			CINC (cq->cq_head, chunk_2, cq->cq_len);
		}
		return 0;
	}
	return -1;
}


size_t
cqueue_write (fd_t fd, cqueue_t *cq)
{
	/* Scrive piu' dati possibile sul file descriptor fd dalla coda cq.
	 * Ritorna il numero di byte spediti (0 o piu').
	 * In caso di errore imposta l'errno come quello di send, altrimenti a
	 * zero. */

	ssize_t nwrite;
	size_t nsent;
	size_t chunk;

	assert (fd > 0);
	assert (cq != NULL);
	assert (cqueue_get_used (cq) > 0);
	/* TODO assert (NONBLOCK); */

	chunk = cqueue_get_used_chunk (cq);
	assert (chunk > 0);

	/* Questo ciclo viene eseguito al massimo due volte: se la coda ha
	 * ciclato e la prima iterazione scrive tutti i dati dalla testa alla
	 * fine del buffer, prova a scrivere anche i dati dall'inizio del
	 * buffer alla coda. */
	nsent = 0;
	do {
		nwrite = send (fd, &(cq->cq_data[cq->cq_head]), chunk,
				MSG_NOSIGNAL);
		assert (nwrite != 0);
		if (nwrite > 0) {
			nsent += nwrite;
			CINC (cq->cq_head, nwrite, cq->cq_len);
			if (cq->cq_head == 0) {
				assert (cq->cq_wrap);
				cq->cq_wrap = FALSE;
				/* Se chunk > 0 vinciamo un altro giro. */
				chunk = cqueue_get_used_chunk (cq);
			}
		}
	} while ((nwrite > 0 && cq->cq_head == 0 && chunk > 0)
	          || (nwrite == -1 && errno == EINTR));

	/* Pulisce il valore di errno se tutto e' andato liscio. */
	if (nwrite > 0)
		errno = 0;

	return nsent;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static size_t
cqueue_get_aval_chunk (const cqueue_t *cq)
{
	assert (cq != NULL);
	return (cq->cq_wrap ?
		cq->cq_head :
		cq->cq_len) - cq->cq_tail;
}


static size_t
cqueue_get_used_chunk (const cqueue_t *cq)
{
	assert (cq != NULL);
	return (cq->cq_wrap ?
		cq->cq_len :
		cq->cq_tail) - cq->cq_head;
}
