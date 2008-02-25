#ifndef RQUEUE_H
#define RQUEUE_H

#include "h/types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

int
rqueue_add (rqueue_t *rq, struct segwrap *sw);


bool
rqueue_can_read (rqueue_t *rq);


bool
rqueue_can_write (rqueue_t *rq);


rqueue_t *
rqueue_create (size_t len);


struct segwrap *
rqueue_cut_unsent (rqueue_t *rq);


void
rqueue_destroy (rqueue_t *rq);


size_t
rqueue_get_aval (rqueue_t *rq);


size_t
rqueue_get_used (rqueue_t *rq);


size_t
rqueue_read (fd_t fd, rqueue_t *rq);


struct segwrap *
rqueue_remove (rqueue_t *rq);


void
rqueue_rm_acked (rqueue_t *rq, struct segwrap *sw);


size_t
rqueue_write (fd_t fd, rqueue_t *rq);


#endif /* RQUEUE_H */
