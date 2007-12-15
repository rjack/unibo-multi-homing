#ifndef TIMEOUT_MANAGER_H
#define TIMEOUT_MANAGER_H

#include "h/types.h"

/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
add_timeout (timeout_t *to, timeout_class class);


double
check_timeouts (void);


void
init_timeout_module (void);

#endif /* TIMEOUT_MANAGER_H */
