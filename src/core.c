#include "h/channel.h"
#include "h/crono.h"
#include "h/conn_mng.h"
#include "h/proxy.h"
#include "h/timeout.h"
#include "h/types.h"
#include "h/util.h"

#include <config.h>
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
core (struct proxy *px)
{
	int i;
	int err;
	int rdy;
	fd_t maxfd;
	fd_set rdset;
	fd_set wrset;
	double min_timeout;
	struct timeval tv_timeout;
	struct ack_args *args;

	/*
	 * Preparazione timeout.
	 */

	/* Creazione timeout attività per i canali con il Ritardatore. */
	args = xmalloc (sizeof (struct ack_args));
	args->aa_px = px;
	init_timeout_module (args);
	for (i = 0; i < NETCHANNELS; i++) {
		struct idle_args *args = xmalloc (sizeof (struct idle_args));
		args->ia_px = px;
		args->ia_ch = &(px->p_net[i]);
		px->p_net[i].c_activity = timeout_create (ACTIVITY_TIMEOUT,
							  idle_handler,
							  args, TRUE);
	}

	for (;;) {
		activate_channels (px->p_chptr);

		min_timeout = check_timeouts ();

		feed_upload (px);
		feed_download (px);

		/*
		 * Select
		 */
		do {
			struct timeval *toptr;

			if (min_timeout > 0) {
				toptr = &tv_timeout;
				d2tv (min_timeout, toptr);
			} else {
				toptr = NULL;
			}

			/* Inizializzazione dei set. */
			FD_ZERO (&rdset);
			FD_ZERO (&wrset);

			/* Selezione dei fd in base allo stato dei canali. */
			maxfd = set_file_descriptors (px->p_chptr,
			                              &rdset, &wrset);

			rdy = select (maxfd + 1, &rdset, &wrset, NULL, toptr);
		} while (rdy == -1 && errno == EINTR);

		if (rdy < 0) {
			fprintf (stderr, "Errore irrimediabile select: %s\n",
			         strerror (errno));
			exit (EXIT_FAILURE);
		}

		/*
		 * Gestione eventi.
		 */
		if (rdy > 0 ) for (i = 0; i < CHANNELS; i++) {
			struct chan *ch;
			ch = px->p_chptr[i];

			/* Connessione da concludere. */
			if (channel_is_connecting (ch)
			    && FD_ISSET (ch->c_sockfd, &wrset)) {
				err = finalize_connection (ch);
				if (err) {
					channel_invalidate (ch);
				} else {
					proxy_prepare_io (px, i);
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
					proxy_prepare_io (px, i);
					printf ("Canale %s, connessione "
					        "accettata.\n",
						channel_name (ch));
				}
			}

			else {
				/* Dati da leggere. */
				if (channel_is_connected (ch)
				    && FD_ISSET (ch->c_sockfd, &rdset)) {
					ssize_t nread;
					nread = channel_read (ch);
					/* FIXME controllo errore decente! */
					assert (nread >= 0);
				}

				/* Dati da scrivere. */
				if (channel_is_connected (ch)
				    && FD_ISSET (ch->c_sockfd, &wrset)) {
					ssize_t nwrite;
					nwrite = channel_write (ch);
					/* FIXME controllo errore decente! */
					assert (nwrite >= 0);
				}
			}
		}
	}
	return 0;
}
