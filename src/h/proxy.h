#ifndef PROXY_H
#define PROXY_H

#include "h/types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
move_data_net2host (struct proxy *px);


void
move_data_host2net (struct proxy *px);


void
idle_handler (void *args);


void
proxy_init (struct proxy *px);


void
proxy_prepare_io (struct proxy *px, int chanid);


#endif /* PROXY_H */
