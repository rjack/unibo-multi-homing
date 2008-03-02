#include "h/types.h"
#include "h/segment.h"
#include "h/channel.h"
#include "h/cqueue.h"
#include "h/crono.h"
#include "h/seghash.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>
#include <stdio.h>

#define     TYPE     struct segwrap
#define     NEXT     sw_next
#define     PREV     sw_prev
#define     EMPTYQ   NULL
#include "src/queue_template"


/*******************************************************************************
			       Variabili locali
*******************************************************************************/

/* Coda dei segwrap inutilizzati. */
static struct segwrap *swcache;
/* Tabella hash dei segwrap spediti. */
#define     HT_SENT_SIZE     10
static struct segwrap *ht_sent[HT_SENT_SIZE];

static bool init_done = FALSE;


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int urgcmp (struct segwrap *sw_1, struct segwrap *sw_2);
static void handle_rcvd_ack (struct segwrap *ack);
static void handle_rcvd_nak (struct segwrap *nak);
static void handle_rcvd_data (struct segwrap *sw);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
handle_rcvd_segment (struct segwrap *rcvd)
{
	seq_t seq;

	assert (rcvd != NULL);

	seq = seg_seq (rcvd->sw_seg);

	if (seg_is_nak (rcvd->sw_seg)) {
		handle_rcvd_nak (rcvd);
		rcvd->sw_seg[SEQ]--;
		handle_rcvd_ack (rcvd);
		segwrap_destroy (rcvd);
	} else if (seg_is_ack (rcvd->sw_seg)) {
		handle_rcvd_ack (rcvd);
		segwrap_destroy (rcvd);
	} else {
#ifndef NDEBUG
		fprintf (stdout, "RCV %sSEG %d\n",
				seg_is_critical (rcvd->sw_seg) ?
					"CRITICAL " : "",
				seg_seq (rcvd->sw_seg));
		fflush (stdout);
#endif /* NDEBUG */
		assert (seg_pld (rcvd->sw_seg) != NULL);
		join_add (rcvd);
	}
}


void
handle_sent_segment (struct segwrap *sent)
{
	assert (sent != NULL);

	if (seg_pld (sent->sw_seg) == NULL)
		segwrap_destroy (sent);
	else {
		struct segwrap *old;

		/* ht_sent non deve contenere due segwrap con lo stesso
		 * seqnum. */
		old = seghash_remove (ht_sent, HT_SENT_SIZE,
				seg_seq (sent->sw_seg));
		if (old != NULL)
			segwrap_destroy (old);
		seghash_add (ht_sent, HT_SENT_SIZE, sent);
	}
}


void
init_segment_module (void)
{
	assert (init_done == FALSE);

	swcache = newQueue ();
	seghash_init (ht_sent, HT_SENT_SIZE);

	init_done = TRUE;
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
	if (seg[FLG] & PLDFLAG)
		return (seg[FLG] & LENFLAG ? &seg[LEN + 1] : &seg[LEN]);
	assert (seg_pld_len (seg) == 0);
	return NULL;
}


len_t
seg_pld_len (seg_t *seg)
{
	/* Ritorna il valore del campo len di seg, oppure 0 se il payload e'
	 * assente. */

	assert (seg != NULL);
	if (seg[FLG] & PLDFLAG)
		return (seg[FLG] & LENFLAG ? seg[LEN] : PLDDEFLEN);
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
segwrap_create (void)
{
	/* Ritorna un nuovo segwrap, recuperandolo dalla cache di quelli
	 * inutilizzati oppure, se questa e' vuota, allocandone uno nuovo.
	 * Il segwrap viene marcato con il timestamp dell'istante attuale. */

	struct segwrap *newsw;
	static struct timeval now;

	assert (init_done == TRUE);

	if (isEmpty (swcache)) {
		newsw = xmalloc (sizeof (struct segwrap));
		newsw->sw_prev = NULL;
		newsw->sw_next = NULL;
	} else
		newsw = qdequeue (&swcache);

	/* Timestamp. */
	gettime (&now);
	newsw->sw_tstamp = tv2d (&now, FALSE);

	return newsw;
}


struct segwrap *
segwrap_nak_create (seq_t nakseq)
{
	struct segwrap *nak;

	nak = segwrap_create ();
	nak->sw_seg[FLG] = 0 | NAKFLAG;
	nak->sw_seg[SEQ] = nakseq;
	nak->sw_seglen = NAKLEN;

	return nak;
}


void
segwrap_destroy (struct segwrap *sw)
{
	assert (init_done == TRUE);
	qenqueue (&swcache, sw);
}


void
segwrap_fill (struct segwrap *sw, cqueue_t *src, len_t pldlen, seq_t seqnum)
{
	/* Riempe il segmento del segwrap sw con i dati presi dalla coda src.
	 * Il segmento avra' payload lungo pldlen, il numero di sequenza
	 * seqnum e le flag PLDFLAG e LENFLAG appropriate. */

	int err;
	pld_t *pld;

	assert (sw != NULL);
	assert (src != NULL);
	assert (pldlen > 0);
	assert (cqueue_get_used (src) >= pldlen);

	/* Flags. */
	sw->sw_seg[FLG] = 0 | PLDFLAG;
	if (pldlen != PLDDEFLEN) {
		/* Payload non standard, flag e campo len. */
		sw->sw_seg[FLG] |= LENFLAG;
		sw->sw_seg[LEN] = pldlen;
	}
	/* Seqnum. */
	sw->sw_seg[SEQ] = seqnum;
	/* Payload. */
	pld = seg_pld (sw->sw_seg);
	err = cqueue_remove (src, pld, pldlen);
	assert (!err);
	/* Seqlen. */
	sw->sw_seglen = FLGLEN + SEQLEN
		+ (pldlen == PLDDEFLEN ? 0 : LENLEN) + pldlen;
}


void
segwrap_flush_cache (void)
{
	struct segwrap *cur;

	while ((cur = qdequeue (&swcache)) != NULL)
		xfree (cur);

	assert (isEmpty (swcache));
}


bool
segwrap_is_acked (struct segwrap *sw, struct segwrap *ack)
{
	if (segwrap_seqcmp (sw, ack) <= 0)
		return TRUE;
	return FALSE;
}


int
segwrap_prio (struct segwrap *sw)
{
	/* Ritorna
	 * 0 se sw e' un NAK
	 * 1 se sw e' un segmento dati da rispedire
	 * 2 se sw e' un ACK
	 * 3 se sw e' un segmento dati. */

	if (seg_is_nak (sw->sw_seg))
		return 0;
	if (seg_is_ack (sw->sw_seg))
		return 2;

	assert (seg_pld (sw->sw_seg) != NULL);
	if (seg_is_critical (sw->sw_seg))
		return 1;
	return 3;
}


int
segwrap_seqcmp (struct segwrap *sw_1, struct segwrap *sw_2)
{
	assert (sw_1 != NULL);
	assert (sw_2 != NULL);
	return seqcmp (seg_seq (sw_1->sw_seg), seg_seq (sw_2->sw_seg));
}


int
segwrap_urgcmp (struct segwrap *sw_1, struct segwrap *sw_2)
{
	/* Ritorna -1 se sw_1 e' piu' urgente di sw_2, 1 altrimenti.
	 * L'ordine di urgenza per tipo e'dato da segwrap_prio.
	 * A parita' di tipo e' piu' urgente quello con timestamp minore.
	 * A parita' di timestamp, quello con il seqnum minore. */

	/* Controllo priorita'. */
	if (segwrap_prio (sw_1) < segwrap_prio (sw_2))
		return -1;
	if (segwrap_prio (sw_1) > segwrap_prio (sw_2))
		return 1;

	/* Priorita' identica, controllo timestamp. */
	if (sw_1->sw_tstamp < sw_2->sw_tstamp)
		return -1;
	if (sw_1->sw_tstamp > sw_2->sw_tstamp)
		return 1;

	/* Timestamp identico, controllo seqnum. */
	assert (segwrap_seqcmp (sw_1, sw_2) != 0);
	return segwrap_seqcmp (sw_1, sw_2);
}


int
seqcmp (seq_t a, seq_t b)
{
	/* Ritorna -1 o 1 a seconda che b rispettivamente segua o preceda a
	 * nell'ordine dei numeri di sequenza. Se a == b ritorna 0. */

	if (a == b)
		return 0;

	if ((a < b && (b - a) > (SEQMAX / 2))
	     || (a > b && (a - b) < (SEQMAX / 2)))
		return 1;

	return -1;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static void
handle_rcvd_ack (struct segwrap *ack)
{
	/* Rimuove e dealloca tutti i segmenti con seqnum minore o uguale ad
	 * ack da tutte le strutture dati del proxy. */

	assert (init_done);

#ifndef NDEBUG
	fprintf (stdout, "RCV ACK %d\n", seg_seq (ack->sw_seg));
	fflush (stdout);
#endif /* NDEBUG */

	seghash_rm_acked (ht_sent, HT_SENT_SIZE, ack);
	urgent_rm_acked (ack);
	netsndbuf_rm_acked (ack);
}


static void
handle_rcvd_nak (struct segwrap *nak)
{
	/* Recupera il segmento con il seqnum indicato dal nak e lo
	 * aggiunge ai segmenti urgenti, dopo aver impostato CRTFLAG. */

	struct segwrap *urg;

#ifndef NDEBUG
	fprintf (stdout, "RCV NAK %d\n", seg_seq (nak->sw_seg));
	fflush (stdout);
#endif /* NDEBUG */

	urg = seghash_remove (ht_sent, HT_SENT_SIZE, seg_seq (nak->sw_seg));
	if (urg != NULL) {
		urg->sw_seg[FLG] |= CRTFLAG;
		urgent_add (urg);
	}
}
