#include "h/types.h"
#include "h/segment.h"
#include "h/util.h"

#include <assert.h>
#include <config.h>


/*******************************************************************************
			       Variabili locali
*******************************************************************************/

/* Tutti i segmenti disponibili preallocati. */
static struct segwrap sgmt[SEQMAX];

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


struct segwrap *
segwrap_get (seq_t seq, len_t pldlen)
{
	assert (pldlen > 0);

	sgmt[seq].sw_seg[FLG] = 0 | PLDFLAG;
	sgmt[seq].sw_seg[SEQ] = seq;
	if (pldlen != PLDDEFLEN) {
		sgmt[seq].sw_seg[LEN] = pldlen;
		sgmt[seq].sw_seg[FLG] |= LENFLAG;
	}

	sgmt[seq].sw_seglen = FLGLEN + SEQLEN
		+ (pldlen == PLDDEFLEN ? 0 : LENLEN) + pldlen;

	sgmt[seq].sw_prev = NULL;
	sgmt[seq].sw_next = NULL;

	return &sgmt[seq];
}
