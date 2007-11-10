#include "h/cqueue.h"
#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <string.h>


/*******************************************************************************
				    Macro
*******************************************************************************/

/* Incremento circolare. */
#define     CINC(x,inc,len)     ((x) = ((x) + (inc)) % (len))


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
cqueue_read (fd_t fd, cqueue_t *cq, size_t nbytes) {
	/* TODO */
	return -1;
}


int
cqueue_remove (cqueue_t *cq, char *buf, size_t buflen) {
	/* TODO */
	return -1;
}


int
cqueue_write (fd_t fd, cqueue_t *cq, size_t nbytes) {
	/* TODO */
	return -1;
}
