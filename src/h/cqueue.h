#ifndef CQUEUE_H
#define CQUEUE_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

int
cqueue_add (cqueue_t *cq, char *buf, size_t nbytes);


size_t
cqueue_get_aval (cqueue_t *cq);


size_t
cqueue_get_used (cqueue_t *cq);


void
cqueue_init (cqueue_t *cq, size_t len);


int
cqueue_read (fd_t fd, cqueue_t *cq, size_t nbytes);


int
cqueue_remove (cqueue_t *cq, char *buf, size_t buflen);


int
cqueue_write (fd_t fd, cqueue_t *cq, size_t nbytes);


#endif /* CQUEUE_H */
