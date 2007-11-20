#ifndef PROXY_H
#define PROXY_H

#include "h/types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
proxy_init (struct proxy *px);


void
proxy_create_buffers (struct proxy *px, int chanid);


#endif /* PROXY_H */
