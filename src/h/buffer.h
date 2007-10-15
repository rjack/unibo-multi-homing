#ifndef BUFFER_H
#define BUFFER_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

size_t
buffer_available (struct buffer *buf);


void
buffer_destroy (struct buffer *buf);


void
buffer_init (struct buffer *buf, size_t len, bool resizable);


int
buffer_read (fd_t fd, struct buffer *buf);


#endif /* BUFFER_H */
