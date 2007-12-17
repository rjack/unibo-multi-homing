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
del_timeout (timeout_t *to, timeout_class class);


void
init_timeout_module (struct ack_args *args);


timeout_t *
timeout_create
(double maxval, timeout_handler_t trigger, void *trigger_args, bool oneshot);


void
timeout_destroy (timeout_t *to);


void
timeout_init (timeout_t *to, double maxval, timeout_handler_t trigger,
              void *trigger_args, bool oneshot);


void
timeout_reset (timeout_t *to);

#endif /* TIMEOUT_MANAGER_H */
