#ifndef CQUEUE_H
#define CQUEUE_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

int
cqueue_add (cqueue_t *cq, seg_t *buf, size_t nbytes);


bool
cqueue_can_read (void *arg);


bool
cqueue_can_write (void *arg);


cqueue_t *
cqueue_create (size_t len);


void
cqueue_destroy (cqueue_t *cq);


void
cqueue_drop (cqueue_t *cq, size_t nbytes);


size_t
cqueue_get_aval (cqueue_t *cq);


size_t
cqueue_seglen (cqueue_t *cq);


size_t
cqueue_get_used (cqueue_t *cq);


int
cqueue_push (cqueue_t *cq, seg_t *buf, size_t nbytes);


size_t
cqueue_read (fd_t fd, cqueue_t *cq);


int
cqueue_remove (cqueue_t *cq, seg_t *buf, size_t buflen);


size_t
cqueue_write (fd_t fd, cqueue_t *cq);


#endif /* CQUEUE_H */
