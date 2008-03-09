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


struct segwrap *
segwrap_clone (struct segwrap *sw);


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


bool
segwrap_is_assigned (struct segwrap *sw);


bool
segwrap_is_acked (struct segwrap *sw, struct segwrap *ack);


bool
segwrap_is_clonable (struct segwrap *sw);


int
segwrap_prio (struct segwrap *sw);


void
segwrap_print (struct segwrap *sw);


int
segwrap_seqcmp (struct segwrap *sw_1, struct segwrap *sw_2);


int
segwrap_urgcmp (struct segwrap *sw_1, struct segwrap *sw_2);


int
seqcmp (seq_t a, seq_t b);

#endif /* SEGMENT_H */
