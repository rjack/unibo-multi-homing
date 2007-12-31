#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <config.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

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
	assert (seg != NULL);
	if (seg[FLG] & PLDFLAG) {
		return (seg[FLG] & LENFLAG ? &seg[LEN + 1] : &seg[LEN]);
	}
	return NULL;
}


len_t
seg_pld_len (seg_t *seg)
{
	assert (seg != NULL);
	if (seg[FLG] & PLDFLAG) {
		return (seg[FLG] & LENFLAG ? seg[LEN] : PLDDEFLEN);
	}
	return 0;
}


seq_t
seg_seq (seg_t *seg)
{
	assert (seg != NULL);
	return seg[SEQ];
}


struct segwrap *
segwrap_create (seq_t seq, len_t pldlen)
{
	struct segwrap *newsw;
	size_t seglen;

	assert (pldlen > 0);

	seglen = FLGLEN + SEQLEN + (pldlen == PLDDEFLEN ? 0 : LENLEN)
	         + pldlen;

	newsw = xmalloc (sizeof(struct segwrap));
	newsw->sw_seg = xmalloc (seglen);

	newsw->sw_seg[FLG] = 0 | PLDFLAG;
	newsw->sw_seg[SEQ] = seq;
	if (pldlen != PLDDEFLEN) {
		newsw->sw_seg[LEN] = pldlen;
		newsw->sw_seg[FLG] |= LENFLAG;
	}

	newsw->sw_seglen = seglen;
	newsw->sw_prev = NULL;
	newsw->sw_next = NULL;

	return newsw;
}
