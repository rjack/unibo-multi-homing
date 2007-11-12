#include "h/cqueue.h"
#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


/*******************************************************************************
				    Macro
*******************************************************************************/

/* Incremento circolare. */
#define     CINC(x,inc,len)     ((x) = ((x) + (inc)) % (len))


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static size_t cqueue_get_aval_chunk (const cqueue_t *cq);
static size_t cqueue_get_used_chunk (const cqueue_t *cq);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
cqueue_add (cqueue_t *cq, char *buf, size_t nbytes) {
	size_t chunk_1;
	size_t chunk_2;

	assert (cq != NULL);
	assert (buf != NULL);
	assert (nbytes > 0);

	if (cqueue_get_aval (cq) >= nbytes) {
		assert (cq->cq_wrap || cq->cq_tail >= cq->cq_head);
		assert (!cq->cq_wrap || cq->cq_tail < cq->cq_head);

		chunk_1 = (cq->cq_wrap ?
		           cq->cq_head - cq->cq_tail :
		           cq->cq_len - cq->cq_tail);
		chunk_2 = nbytes - chunk_1;

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


size_t
cqueue_get_aval (cqueue_t *cq) {
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
cqueue_get_used (cqueue_t *cq) {
	assert (cq != NULL);
	return (cq->cq_len - cqueue_get_aval (cq));
}


void
cqueue_init (cqueue_t *cq, size_t len) {
	assert (cq != NULL);
	assert (len > 0);

	cq->cq_data = xmalloc (len * sizeof (char));
	cq->cq_len = len;
	cq->cq_head = 0;
	cq->cq_tail = 0;
	cq->cq_wrap = FALSE;
}


int
cqueue_read (fd_t fd, cqueue_t *cq) {
	ssize_t nread;
	size_t chunk;

	assert (fd > 0);
	assert (cq != NULL);
	assert (cqueue_get_aval (cq) > 0);
	/* TODO assert (NONBLOCK); */

	chunk = cqueue_get_aval_chunk (cq);
	assert (chunk > 0);

	do {
		nread = read (fd, &cq->cq_data[cq->cq_tail], chunk);
		if (nread > 0) {
			CINC (cq->cq_tail, nread, cq->cq_len);
			if (cq->cq_tail == 0) {
				assert (!cq->cq_wrap);
				cq->cq_wrap = TRUE;
			}
			chunk = cqueue_get_aval_chunk (cq);
		}
	} while ((nread > 0 && chunk > 0)
	          || (nread == -1 && errno == EINTR));

	if ((nread == -1 && errno == EAGAIN)
	     || nread > 0) {
		nread = 1;
	}
	return nread;
}


int
cqueue_remove (cqueue_t *cq, char *buf, size_t nbytes) {
	size_t chunk_1;
	size_t chunk_2;

	assert (cq != NULL);
	assert (buf != NULL);
	assert (nbytes > 0);

	if (cqueue_get_used (cq) >= nbytes) {
		assert (cq->cq_wrap || cq->cq_tail > cq->cq_head);
		assert (!cq->cq_wrap || cq->cq_tail <= cq->cq_head);

		chunk_1 = (cq->cq_wrap ?
		           cq->cq_len - cq->cq_head :
		           cq->cq_tail - cq->cq_head);
		chunk_2 = nbytes - chunk_1;

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
cqueue_write (fd_t fd, cqueue_t *cq) {
	ssize_t nwrite;
	ssize_t chunk;

	assert (fd > 0);
	assert (cq != NULL);
	assert (cqueue_get_used (cq) > 0);
	/* TODO assert (NONBLOCK); */

	chunk = cqueue_get_used_chunk (cq);
	assert (chunk > 0);

	do {
		nwrite = send (fd, &cq->cq_data[cq->cq_head], chunk,
			       MSG_NOSIGNAL);
		if (nwrite > 0) {
			CINC (cq->cq_head, nwrite, cq->cq_len);
			if (cq->cq_head == 0) {
				assert (cq->cq_wrap);
				cq->cq_wrap = FALSE;
			}
			chunk = cqueue_get_used_chunk (cq);
		}
	} while ((nwrite > 0 && chunk > 0)
	          || (nwrite == -1 && errno == EINTR));

	if ((nwrite == -1 && errno == EAGAIN)
	     || nwrite > 0) {
		nwrite = 1;
	}
	return nwrite;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static size_t
cqueue_get_aval_chunk (const cqueue_t *cq) {
	assert (cq != NULL);
	return (cq->cq_wrap ?
		cq->cq_head :
		cq->cq_len) - cq->cq_tail;
}


static size_t
cqueue_get_used_chunk (const cqueue_t *cq) {
	assert (cq != NULL);
	return (cq->cq_wrap ?
		cq->cq_len :
		cq->cq_tail) - cq->cq_head;
}
