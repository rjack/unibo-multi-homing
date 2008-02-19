#ifndef SEGMENT_H
#define SEGMENT_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
handle_rcvd_segment (struct segwrap *sw);


void
handle_sent_segment (struct segwrap *sent);


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


int
segwrap_cmp (struct segwrap *sw_1, struct segwrap *sw_2);


struct segwrap *
segwrap_create (void);


struct segwrap *
segwrap_nak_create (seq_t nakseq);


void
segwrap_destroy (struct segwrap *sw);


void
segwrap_fill (struct segwrap *sw, cqueue_t *src, len_t pldlen, seq_t seqnum);


void
segwrap_flush_cache (void);


int
seqcmp (seq_t a, seq_t b);


void
urgent_add (struct segwrap *sw);


bool
urgent_empty (void);


struct segwrap *
urgent_head (void);


struct segwrap *
urgent_remove (void);

#endif /* SEGMENT_H */
