#ifndef RQUEUE_H
#define RQUEUE_H

#include "h/types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

int
rqueue_add (rqueue_t *rq, struct segwrap *sw);


void
rqueue_add_nak (rqueue_t *rq, seq_t seq);


bool
rqueue_can_read (void *arg);


bool
rqueue_can_write (void *arg);


rqueue_t *
rqueue_create (size_t len);


void
rqueue_destroy (rqueue_t *rq);


size_t
rqueue_get_aval (rqueue_t *rq);


size_t
rqueue_get_used (rqueue_t *rq);


int
rqueue_read (fd_t fd, rqueue_t *rq);


struct segwrap *
rqueue_remove (rqueue_t *rq);


int
rqueue_write (fd_t fd, rqueue_t *rq);


#endif /* RQUEUE_H */
