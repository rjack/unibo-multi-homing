#include "h/types.h"
#include "h/util.h"
#include "h/cqueue.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
proxy_init (struct proxy *px) {
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
proxy_create_buffers (struct proxy *px, int chanid) {
	/* Crea i buffer di I/O relativi al canale chanid. */

	assert (px != NULL);
	assert (chanid >= 0);
	assert (chanid < CHANNELS);

	if (chanid == HOST) {
		size_t buflen;
		assert (px->p_chptr[chanid] == &px->p_host);

		buflen = tcp_get_buffer_size (px->p_host.c_sockfd,
		                              SO_RCVBUF);
		px->p_host_rcvbuf = cqueue_create (buflen);

		buflen = tcp_get_buffer_size (px->p_host.c_sockfd,
		                              SO_SNDBUF);
		px->p_host_sndbuf = cqueue_create (buflen);
	}
	/* NET */
	else {
		/* TODO */
	}
}
