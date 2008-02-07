#include "h/types.h"
#include "h/segment.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
init_segment_module (void)
{
	/* TODO init_segment_module. */
}


bool
seg_is_ack (seg_t *seg)
{
	assert (seg != NULL);
	return (seg[FLG] & ACKFLAG ? TRUE : FALSE);
}


bool
seg_is_critical (seg_t *seg)
{
	assert (seg != NULL);
	return (seg[FLG] & CRTFLAG ? TRUE : FALSE);
}


bool
seg_is_nak (seg_t *seg)
{
	assert (seg != NULL);
	return (seg[FLG] & NAKFLAG ? TRUE : FALSE);
}


pld_t *
seg_pld (seg_t *seg)
{
	/* Ritorna il puntatore al campo pld di seg, oppure NULL se il payload
	 * e' assente. */

	assert (seg != NULL);
	if (seg[FLG] & PLDFLAG) {
		return (seg[FLG] & LENFLAG ? &seg[LEN + 1] : &seg[LEN]);
	}
	assert (seg_pld_len (seg) == 0);
	return NULL;
}


len_t
seg_pld_len (seg_t *seg)
{
	/* Ritorna il valore del campo len di seg, oppure 0 se il payload e'
	 * assente. */

	assert (seg != NULL);
	if (seg[FLG] & PLDFLAG) {
		return (seg[FLG] & LENFLAG ? seg[LEN] : PLDDEFLEN);
	}
	assert (seg_pld (seg) == NULL);
	return 0;
}


seq_t
seg_seq (seg_t *seg)
{
	/* Ritorna il numero di sequenza di seg. */

	assert (seg != NULL);
	return seg[SEQ];
}
