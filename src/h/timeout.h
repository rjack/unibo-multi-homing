#ifndef TIMEOUT_MANAGER_H
#define TIMEOUT_MANAGER_H

#include "h/types.h"

/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
add_timeout (timeout_t *to, int class);


void
add_nak_timeout (seq_t seq);


double
check_timeouts (void);


void
del_timeout (timeout_t *to, int class);


void
del_nak_timeout (seq_t seq);


void
enable_ping_timeout (void);


timeout_t *
get_timeout (int class, int id);


void
init_timeout_module (void);


timeout_t *
timeout_create
(double maxval, timeout_handler_t trigger, int trigger_arg, bool oneshot);


void
timeout_destroy (timeout_t *to);


void
timeout_init (timeout_t *to, double maxval, timeout_handler_t trigger,
		int trigger_arg, bool oneshot);


void
timeout_reset (timeout_t *to);

#endif /* TIMEOUT_MANAGER_H */
