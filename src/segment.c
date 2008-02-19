#include "h/types.h"
#include "h/segment.h"
#include "h/channel.h"
#include "h/cqueue.h"
#include "h/crono.h"
#include "h/seghash.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>

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
/* Coda dei segwrap urgenti. */
static struct segwrap *urg;
/* Coda dei segmenti da ricostruire per spedirli all'host. */
static struct segwrap *join;
/* Tabella hash dei segwrap spediti. */
#define     HT_SENT_SIZE     10
static struct segwrap *ht_sent[HT_SENT_SIZE];

static bool init_done = FALSE;


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int urgcmp (struct segwrap *sw_1, struct segwrap *sw_2);
static void handle_rcvd_ack (seq_t ack);
static void handle_rcvd_nak (seq_t nak);
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
		handle_rcvd_nak (seq);
		handle_rcvd_ack (seq - 1);
		segwrap_destroy (rcvd);
	} else if (seg_is_ack (rcvd->sw_seg)) {
		handle_rcvd_ack (seq);
		segwrap_destroy (rcvd);
	} else {
		assert (seg_pld (rcvd->sw_seg) != NULL);
		handle_rcvd_data (rcvd);
	}
}


void
handle_sent_segment (struct segwrap *sent)
{
	assert (sent != NULL);

	if (seg_pld (sent->sw_seg) == NULL)
		segwrap_destroy (sent);
	else
		seghash_add (ht_sent, HT_SENT_SIZE, sent);
}


void
init_segment_module (void)
{
	assert (init_done == FALSE);

	swcache = newQueue ();
	urg = newQueue ();
	join = newQueue ();
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


int
segwrap_cmp (struct segwrap *sw_1, struct segwrap *sw_2)
{
	assert (sw_1 != NULL);
	assert (sw_2 != NULL);
	return seqcmp (seg_seq (sw_1->sw_seg), seg_seq (sw_2->sw_seg));
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
	assert (cqueue_get_used (src) > 0);

	/* Flags. */
	sw->sw_seg[FLG] = 0 | PLDFLAG;
	if (pldlen != PLDDEFLEN)
		sw->sw_seg[FLG] |= LENFLAG;
	/* Seqnum. */
	sw->sw_seg[SEQ] = seqnum;
	/* Payload. */
	pld = seg_pld (sw->sw_seg);
	err = cqueue_remove (src, pld, pldlen);
	assert (!err);
}


void
segwrap_flush_cache (void)
{
	struct segwrap *cur;

	while ((cur = qdequeue (&swcache)) != NULL)
		xfree (cur);

	assert (isEmpty (swcache));
}


int
seqcmp (seq_t a, seq_t b)
{
	/* Ritorna -1 o 1 a seconda che b rispettivamente preceda o segua a
	 * nell'ordine dei numeri di sequenza. Se a == b ritorna 0. */

	if (a == b)
		return 0;

	if ((a < b && (b - a) > (SEQMAX / 2))
	     || (a > b && (a - b) < (SEQMAX / 2)))
		return -1;

	return 1;
}


void
urgent_add (struct segwrap *sw)
{
	/* Aggiunge sw alla struttura dati dei segmenti urgenti, i piu'
	 * urgenti prima.
	 * XXX costo O(n). Vale la pena di usare una priority queue basata su
	 * XXX heap? */

	assert (init_done == TRUE);
	assert (sw != NULL);
	assert (!seg_is_nak (sw->sw_seg));
	assert (!seg_is_ack (sw->sw_seg));

	qinorder_insert (&urg, sw, &urgcmp);
}


bool
urgent_empty (void)
{
	/* Ritorna TRUE se la struttura dei segmenti urgenti e' vuota, FALSE
	 * altrimenti. */

	assert (init_done == TRUE);
	return isEmpty (urg);
}


struct segwrap *
urgent_head (void)
{
	/* Ritorna il segmento in testa alla coda dei segmenti urgenti senza
	 * rimuoverlo, oppure NULL se la coda e' vuota. */

	assert (init_done == TRUE);
	return getHead (urg);
}


struct segwrap *
urgent_remove (void)
{
	/* Rimuove e ritorna un segwrap dalla struttura dati dei segmenti
	 * urgenti, in ordine di urgenza.
	 * Se la coda e' vuota ritorna NULL.  */

	assert (init_done == TRUE);
	return qdequeue (&urg);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static int
urgcmp (struct segwrap *sw_1, struct segwrap *sw_2)
{
	/* Ritorna -1 se sw_1 e' piu' urgente di sw_2, 1 altrimenti. */

	int res;

	if (sw_1->sw_tstamp < sw_2->sw_tstamp)
		return -1;
	if (sw_1->sw_tstamp > sw_2->sw_tstamp)
		return 1;
	/* Timestamp identici, confronta il seqnum. */
	res = seqcmp (seg_seq (sw_1->sw_seg), seg_seq (sw_2->sw_seg));
	assert (res != 0);
	return res;
}


static void
handle_rcvd_ack (seq_t ack)
{
	/* TODO handle_rcvd_ack */
}


static void
handle_rcvd_nak (seq_t nak)
{
	/* Recupera il segwrap con il seqnum nak e lo aggiunge alla coda
	 * urgente. Imposta la riorganizzazione di tutte le code di upload
	 * alla prossima feed_upload, in modo che il segwrap abbia la giusta
	 * collocazione. */

	struct segwrap *needed;

	needed = seghash_remove (ht_sent, HT_SENT_SIZE, nak);
	if (needed != NULL) {
		/* XXX volendo si puo' frammentare needed qui. */
		urgent_add (needed);
		set_net_upload_reorg ();
	}
}


static void
handle_rcvd_data (struct segwrap *sw)
{
	assert (sw != NULL);
	qinorder_insert (&join, sw, &segwrap_cmp);
}
