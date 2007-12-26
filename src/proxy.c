#include "h/channel.h"
#include "h/cqueue.h"
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


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
do_joining (struct proxy *px)
{
	/* TODO do_joining */
}


void
do_routing (struct proxy *px)
{
	/* Il routing e' banale: un segmento a ogni canale, finche' ci stanno
	 * nei net o si svuota l'host.
	 * XXX round robin = variabile statica che tiene traccia dell'ultimo
	 * canale servito.
	 * XXX solo i canali connessi.
	 * XXX le code dei net devono essere sempre piu' o meno uguali. */

	/* In binario e' 111, ogni canale inattivo o pieno spegne il proprio
	 * bit. */
	int needmask = 0x7;
	/* Contatore, statico per politica round robin. */
	static int id = 0;

	while (needmask != 0x0
	       && cqueue_get_used (px->p_host_rcvbuf) > 0) {
		if (channel_is_connected (&px->p_net[id])
		    /* TODO && c'e' spazio nel buffer */ ) {
			/* TODO dequeue (host) => enqueue (net[id]) */
		} else {
			/* spegnimento bit */
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

	fprintf (stderr, "WARNING Canale %s chiuso per inattività\n",
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

#if !HAVE_MSG_NOSIGNAL
	/* Gestione SIGPIPE.
	 * NetBSD non ha un equivalente di MSG_NOSIGNAL, quindi l'unico modo
	 * e' ignorare il segnale. */
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
	 * Per i canali di rete fa partire i timeout di attività. */

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
		timeout_reset (px->p_net[id].c_activity);
		add_timeout (px->p_net[id].c_activity, ACTIVITY);
		/* TODO buffer canali ritardatore */
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
