#include "h/channel.h"
#include "h/conn_mng.h"
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
core (struct chan *chnl) {
	int i;
	int err;
	int rdy;
	fd_t maxfd;
	fd_set rdset;
	fd_set wrset;

	for (;;) {
		/*
		 * Gestione connessioni.
		 */
		manage_connections (chnl);

		/*
		 * Select
		 */
		do {
			/* Inizializzazione dei set. */
			FD_ZERO (&rdset);
			FD_ZERO (&wrset);

			/* Selezione dei fd in base allo stato dei canali. */
			maxfd = set_file_descriptors (chnl, &rdset, &wrset);

			rdy = select (maxfd + 1, &rdset, &wrset, NULL, NULL);
		} while (rdy == -1 && errno == EINTR);
		if (rdy < 0) {
			fprintf (stderr, "Errore irrimediabile select: %s\n",
			         strerror (errno));
			exit (EXIT_FAILURE);
		}

		/* Gestione eventi. */
		for (i = 0; i < CHANNELS; i++) {
			/* Connessione da concludere. */
			if (channel_is_connecting (&chnl[i])
			    && FD_ISSET (chnl[i].c_sockfd, &wrset)) {
				err = finalize_connection (&chnl[i]);
				if (err) {
					channel_invalidate (&chnl[i]);
				} else {
					printf ("Canale %s connesso.\n",
					        channel_name (&chnl[i]));
				}
			}

			/* Connessione da accettare. */
			else if (channel_is_listening (&chnl[i])
			         && FD_ISSET (chnl[i].c_listfd, &rdset)) {
				err = accept_connection (&chnl[i]);
				if (err) {
					channel_invalidate (&chnl[i]);
				} else {
					printf ("Canale %s, connessione "
					        "accettata.\n",
						channel_name (&chnl[i]));
				}
			}

			else {
				/* Dati da leggere. */
				if (channel_is_connected (&chnl[i])
				    && FD_ISSET (chnl[i].c_sockfd, &rdset)) {
					/* TODO lettura */
					assert (FALSE);
				}

				/* Dati da scrivere. */
				if (channel_is_connected (&chnl[i])
				    && FD_ISSET (chnl[i].c_sockfd, &wrset)) {
					/* TODO scrittura */
					assert (FALSE);
				}
			}
		}
	}
	return 0;
}
