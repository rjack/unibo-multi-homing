#ifndef PROXY_H
#define PROXY_H

#include "h/types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
feed_download (struct proxy *px);


void
feed_upload (struct proxy *px);


void
idle_handler (void *args);


void
proxy_init (struct proxy *px);


void
proxy_prepare_io (struct proxy *px, int chanid);


#endif /* PROXY_H */
