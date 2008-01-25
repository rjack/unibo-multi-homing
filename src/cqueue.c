#include "h/cqueue.h"
#include "h/types.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*******************************************************************************
				    Macro
*******************************************************************************/

/* Incremento e decremento circolare. */
#define     CINC(x,inc,len)     ((x) = ((x) + (inc)) % (len))
#define     CDEC(x,dec,len)     ((x) = ((x) - (dec)) % (len))

#if ! HAVE_MSG_NOSIGNAL
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
	size_t chunk_1;
	size_t chunk_2;

	assert (cq != NULL);
	assert (buf != NULL);
	assert (nbytes > 0);

	if (cqueue_get_aval (cq) >= nbytes) {
		assert (cq->cq_wrap || cq->cq_tail >= cq->cq_head);
		assert (!cq->cq_wrap || cq->cq_tail < cq->cq_head);

		chunk_1 = cqueue_get_aval_chunk (cq);
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
			cq->cq_tail += chunk_2;
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
	if (cqueue_get_aval (cq) > 0) {
		return TRUE;
	}
	return FALSE;
}


bool
cqueue_can_write (void *arg)
{
	cqueue_t *cq;

	assert (arg != NULL);

	cq = (cqueue_t *) arg;
	if (cqueue_get_used (cq) > 0) {
		return TRUE;
	}
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
cqueue_drop (cqueue_t *cq, size_t nbytes)
{
	assert (cq != NULL);
	assert (nbytes > 0);
	assert (nbytes <= cqueue_get_aval (cq));

	CINC (cq->cq_head, nbytes, cq->cq_len);
	if (cq->cq_head <= cq->cq_tail) {
		cq->cq_wrap = FALSE;
	}
}


size_t
cqueue_get_aval (cqueue_t *cq)
{
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
	assert (cq != NULL);
	return (cq->cq_len - cqueue_get_aval (cq));
}


int
cqueue_push (cqueue_t *cq, seg_t *buf, size_t nbytes)
{
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


int
cqueue_read (fd_t fd, cqueue_t *cq)
{
	ssize_t nread;
	size_t chunk;

	assert (fd > 0);
	assert (cq != NULL);
	assert (cqueue_get_aval (cq) > 0);
	/* TODO assert (NONBLOCK); */

	chunk = cqueue_get_aval_chunk (cq);
	assert (chunk > 0);

	/* Questo ciclo viene eseguito al massimo due volte. */
	do {
		nread = read (fd, &cq->cq_data[cq->cq_tail], chunk);
		if (nread > 0) {
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

	if (nread > 0 || (nread == -1 && errno == EAGAIN)) {
		nread = 1;
	}
	return nread;
}


int
cqueue_remove (cqueue_t *cq, seg_t *buf, size_t nbytes)
{
	size_t chunk_1;
	size_t chunk_2;

	assert (cq != NULL);
	assert (buf != NULL);

	if (cqueue_get_used (cq) >= nbytes) {
		assert (cq->cq_wrap || cq->cq_tail > cq->cq_head);
		assert (!cq->cq_wrap || cq->cq_tail <= cq->cq_head);

		chunk_1 = cqueue_get_used_chunk (cq);
		chunk_2 = (nbytes > chunk_1 ? nbytes - chunk_1 : 0);

		memcpy (buf, &cq->cq_data[cq->cq_head], chunk_1);
		CINC (cq->cq_head, chunk_1, cq->cq_len);
		buf += chunk_1;
		if (cq->cq_head == 0) {
			assert (cq->cq_wrap);
			cq->cq_wrap = FALSE;
		}

		if (chunk_2 > 0) {
			memcpy (buf, &cq->cq_data[cq->cq_head], chunk_2);
			cq->cq_head += chunk_2;
		}
		return 0;
	}
	return -1;
}


int
cqueue_write (fd_t fd, cqueue_t *cq)
{
	/* Scrive piu' dati possibile sul file descriptor fd dalla coda cq.
	 * Ritorna il numero di byte scritti, 0 se e' completamente fallita.
	 * Se e' avvenuto un errore imposta l'errno corrispondente, se riesce
	 * imposta errno = 0. */

	ssize_t nsent;
	ssize_t nwrite;
	size_t chunk;

	assert (fd > 0);
	assert (cq != NULL);
	assert (cqueue_get_used (cq) > 0);
	/* TODO assert (NONBLOCK); */

	nsent = 0;
	chunk = cqueue_get_used_chunk (cq);
	assert (chunk > 0);

	/* Questo ciclo viene eseguito al massimo due volte: se la coda ha
	 * ciclato e la prima iterazione scrive tutti i dati dalla testa alla
	 * fine del buffer, prova a scrivere anche i dati dall'inizio del
	 * buffer alla coda. */
	do {
		nwrite = send (fd, &cq->cq_data[cq->cq_head], chunk,
			       MSG_NOSIGNAL);
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

	assert (nwrite != 0);
	/* Pulisce il valore di errno se tutto e' andato liscio. */
	if (nwrite > 0
	    || errno == EAGAIN || errno == EWOULDBLOCK) {
		errno = 0;
	}
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
