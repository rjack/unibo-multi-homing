#ifndef CONN_MNG_H
#define CONN_MNG_H

#include "h/types.h"

#include <sys/select.h>

/*******************************************************************************
				  Prototipi
*******************************************************************************/

int
accept_connection (struct chan *ch);


void
activate_channels (struct proxy *px);


int
finalize_connection (struct chan *ch);


fd_t
set_file_descriptors (struct chan *chnl[CHANNELS],
                      fd_set *rdset, fd_set *wrset);


#endif /* CONN_MNG_H */
