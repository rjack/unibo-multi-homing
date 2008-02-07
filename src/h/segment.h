#ifndef SEGMENT_H
#define SEGMENT_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
init_segment_module (void);


bool
seg_is_ack (seg_t *seg);


bool
seg_is_critical (seg_t *seg);


bool
seg_is_nak (seg_t *seg);


pld_t *
seg_pld (seg_t *seg);


len_t
seg_pld_len (seg_t *seg);


seq_t
seg_seq (seg_t *seg);

#endif /* SEGMENT_H */
