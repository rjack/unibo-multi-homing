#include "h/channel.h"
#include "h/cqueue.h"
#include "h/rqueue.h"
#include "h/segment.h"
#include "h/timeout.h"
#include "h/types.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int host_read (fd_t fd, void *args);
static int host_write (fd_t fd, void *args);
static int net_read (fd_t fd, void *args);
static int net_write (fd_t fd, void *args);
static void mov_host2net (struct proxy *px, int id, len_t pldlen);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
feed_download (struct proxy *px)
{
	/* TODO download */
}


void
feed_upload (struct proxy *px)
{
	/* Il routing e' banale: un segmento a ogni canale, finche' ci stanno
	 * nei net o si svuota l'host.
	 * XXX round robin: id variabile statica che tiene traccia dell'ultimo
	 * XXX canale servito.
	 * XXX solo i canali connessi.
	 * XXX le code dei net devono essere sempre piu' o meno uguali. */

	/* Ogni canale inattivo o pieno spegne il proprio bit. */
	int needmask;
	/* Contatore, statico per politica round robin. */
	static int id = 0;
	size_t used;
	len_t pldlen;

	needmask = 0x7;
	used = cqueue_get_used (px->p_host_rcvbuf),
	pldlen = (used >= PLDDEFLEN ? PLDDEFLEN : used);
	while (needmask != 0x0 && used > 0) {
		if (channel_is_connected (&px->p_net[id])
		    && rqueue_get_aval (px->p_net_sndbuf[id]) >= (HDRMAXLEN
		                                                  + pldlen)) {
			mov_host2net (px, id, pldlen);
			used = cqueue_get_used (px->p_host_rcvbuf),
			pldlen = (used >= PLDDEFLEN ? PLDDEFLEN : used);
		} else {
			/* Spegnimento bit. */
			needmask &= ~(0x1 << id);
		}
		id = (id + 1) % NETCHANNELS;
	}
}


void
idle_handler (void *args)
{
	struct proxy *px;
	struct chan *ch;

	assert (args != NULL);

	px = ((struct idle_args *)args)->ia_px;
	ch = ((struct idle_args *)args)->ia_ch;
	assert (px != NULL);
	assert (ch != NULL);

	xfree (args);
	args = NULL;

	/* TODO travaso buffer */

	fprintf (stderr, "WARNING Canale %s chiuso per inattivita'\n",
	         channel_name (ch));

	channel_invalidate (ch);
}


void
proxy_init (struct proxy *px)
{
	int i;

	assert (px != NULL);

	/* Collegamento puntatori.
	 * L'array p_chptr viene usato dalle funzioni che devono ciclare su
	 * tutti i canali senza distinzione di tipo. */
	for (i = 0; i < NETCHANNELS; i++) {
		px->p_chptr[i] = &px->p_net[i];
	}
	px->p_chptr[i] = &px->p_host;

	px->p_host_rcvbuf = NULL;
	px->p_host_sndbuf = NULL;

	for (i = 0; i < NETCHANNELS; i++) {
		px->p_net_rcvbuf[i] = NULL;
		px->p_net_sndbuf[i] = NULL;
	}

	px->p_outseq = 0;

	/* Ignora SIGPIPE sui sistemi senza MSG_NOSIGNAL. */
#if !HAVE_MSG_NOSIGNAL
	{
		int err;
		struct sigaction act;

		act.sa_handler = SIG_IGN;
		sigemptyset (&act.sa_mask);
		act.sa_flags = 0;
		err = sigaction (SIGPIPE, &act, NULL);
		assert (!err);
		fprintf (stderr,
		         "Manca MSG_NOSIGNAL, tutti i SIGPIPE ignorati.\n");
	}
#endif
}


void
proxy_prepare_io (struct proxy *px, int id)
{
	/* Crea i buffer di I/O relativi al canale id e imposta le
	 * conseguenti condizioni e le funzioni di I/O.
	 *
	 * Per i canali di rete fa partire i timeout di attivita'. */

	assert (px != NULL);
	assert (id >= 0);
	assert (id < CHANNELS);

	if (id == HOST) {
		size_t buflen;
		assert (px->p_chptr[id] == &px->p_host);

		buflen = tcp_get_buffer_size (px->p_host.c_sockfd, SO_RCVBUF);
		px->p_host_rcvbuf = cqueue_create (buflen);
		buflen = tcp_get_buffer_size (px->p_host.c_sockfd, SO_SNDBUF);
		px->p_host_sndbuf = cqueue_create (buflen);

		px->p_host.c_rcvbufptr = px->p_host_rcvbuf;
		px->p_host.c_sndbufptr = px->p_host_sndbuf;

		px->p_host.c_can_read = &cqueue_can_read;
		px->p_host.c_can_write = &cqueue_can_write;
		px->p_host.c_can_read_args = (void *)px->p_host_rcvbuf;
		px->p_host.c_can_write_args = (void *)px->p_host_sndbuf;

		px->p_host.c_read = &host_read;
		px->p_host.c_write = &host_write;
	}
	/* NET */
	else {
		size_t buflen;

		buflen = MAX (SEGMAXLEN,
		              tcp_get_buffer_size (px->p_net[id].c_sockfd,
		                                   SO_RCVBUF));
		px->p_net_rcvbuf[id] = rqueue_create (buflen);
		buflen = 2 * tcp_get_buffer_size (px->p_net[id].c_sockfd,
		                                  SO_SNDBUF);
		px->p_net_sndbuf[id] = rqueue_create (buflen);

		px->p_net[id].c_rcvbufptr = px->p_net_rcvbuf[id];
		px->p_net[id].c_sndbufptr = px->p_net_sndbuf[id];

		px->p_net[id].c_can_read = &rqueue_can_read;
		px->p_net[id].c_can_write = &rqueue_can_write;
		px->p_net[id].c_can_read_args = px->p_net_rcvbuf[id];
		px->p_net[id].c_can_write_args = px->p_net_sndbuf[id];

		px->p_net[id].c_read = &net_read;
		px->p_net[id].c_write = &net_write;

		timeout_reset (px->p_net[id].c_activity);
		add_timeout (px->p_net[id].c_activity, ACTIVITY);
	}
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static int
host_read (fd_t fd, void *args)
{
	assert (fd >= 0);
	assert (args != NULL);

	return cqueue_read (fd, (cqueue_t *)args);
}


static int
host_write (fd_t fd, void *args)
{
	assert (fd >= 0);
	assert (args != NULL);

	return cqueue_write (fd, (cqueue_t *)args);
}


static int
net_read (fd_t fd, void *args)
{
	assert (fd >= 0);
	assert (args != NULL);

	return rqueue_read (fd, (rqueue_t *)args);
}


static int
net_write (fd_t fd, void *args)
{
	assert (fd >= 0);
	assert (args != NULL);

	return rqueue_write (fd, (rqueue_t *)args);
}


static void
mov_host2net (struct proxy *px, int id, len_t pldlen)
{
	int err;
	struct segwrap *newsw;
	pld_t *pld = NULL;

	assert (px != NULL);

	newsw = segwrap_create (px->p_outseq, pldlen);
	px->p_outseq++;

	pld = seg_pld (newsw->sw_seg);
	err = cqueue_remove (px->p_host_rcvbuf, pld, pldlen);
	assert (!err);

	rqueue_add (px->p_net_sndbuf[id], newsw);
}
