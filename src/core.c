#include "h/channel.h"
#include "h/conn_mng.h"
#include "h/proxy.h"
#include "h/types.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
core (struct proxy *px) {
	int i;
	int err;
	int rdy;
	fd_t maxfd;
	fd_set rdset;
	fd_set wrset;

	for (;;) {
		activate_channels (px->p_chptr);

		/*
		 * Select
		 */
		do {
			/* Inizializzazione dei set. */
			FD_ZERO (&rdset);
			FD_ZERO (&wrset);

			/* Chiusura canali bloccati dal Ritardatore. */
			close_idle_channels (px->p_net);

			/* Selezione dei fd in base allo stato dei canali. */
			maxfd = set_file_descriptors (px->p_chptr,
			                              &rdset, &wrset);

			rdy = select (maxfd + 1, &rdset, &wrset, NULL, NULL);
		} while (rdy == -1 && errno == EINTR);
		if (rdy < 0) {
			fprintf (stderr, "Errore irrimediabile select: %s\n",
			         strerror (errno));
			exit (EXIT_FAILURE);
		}

		/*
		 * Gestione eventi.
		 */
		for (i = 0; i < CHANNELS; i++) {
			struct chan *ch;
			ch = px->p_chptr[i];

			/* Connessione da concludere. */
			if (channel_is_connecting (ch)
			    && FD_ISSET (ch->c_sockfd, &wrset)) {
				err = finalize_connection (ch);
				if (err) {
					channel_invalidate (ch);
				} else {
					proxy_create_buffers (px, i);
					printf ("Canale %s connesso.\n",
					        channel_name (ch));
				}
			}

			/* Connessione da accettare. */
			else if (channel_is_listening (ch)
			         && FD_ISSET (ch->c_listfd, &rdset)) {
				err = accept_connection (ch);
				if (err) {
					channel_invalidate (ch);
				} else {
					proxy_create_buffers (px, i);
					printf ("Canale %s, connessione "
					        "accettata.\n",
						channel_name (ch));
				}
			}

			else {
				/* Dati da leggere. */
				if (channel_is_connected (ch)
				    && FD_ISSET (ch->c_sockfd, &rdset)) {
					/* TODO lettura */
					assert (FALSE);
				}

				/* Dati da scrivere. */
				if (channel_is_connected (ch)
				    && FD_ISSET (ch->c_sockfd, &wrset)) {
					/* TODO scrittura */
					assert (FALSE);
				}
			}
		}
	}
	return 0;
}
