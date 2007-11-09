#include "h/types.h"


void
cqueue_init (cqueue_t *cq, size_t len) {
	assert (cq != NULL);
	assert (len > 0);

	cq->cq_data = xmalloc (len * sizeof (char));
	cq->cq_len = len;
	cq->cq_head = 0;
	cq->cq_tail = 0;
}


int
cqueue_add (cqueue_t *cq, char *buf, size_t buflen) {
	/* TODO */
	return -1;
}


int
cqueue_remove (cqueue_t *cq, char *buf, size_t buflen) {
	/* TODO */
	return -1;
}


int
cqueue_read (fd_t fd, cqueue_t *cq, size_t nbytes) {
	/* TODO */
	return -1;
}


int
cqueue_write (fd_t fd, cqueue_t *cq, size_t nbytes) {
	/* TODO */
	return -1;
}
