#ifndef CONN_MNG_H
#define CONN_MNG_H

#include "h/types.h"

#include <sys/select.h>

/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
manage_connections (struct chan* chnl);


fd_t
set_file_descriptors (struct chan *chnl, fd_set *rdset, fd_set *wrset);


#endif /* CONN_MNG_H */
