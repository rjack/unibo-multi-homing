#include "h/channel.h"
#include "h/crono.h"
#include "h/segment.h"
#include "h/timeout.h"
#include "h/types.h"
#include "h/util.h"

#include <config.h>
#include <string.h>
#include <sys/select.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
core (void)
{
	int err;
	int rdy;
	cd_t cd;
	fd_t maxfd;
	fd_set rdset;
	fd_set wrset;
	double min_timeout;
	struct timeval tv_timeout;

	/* DEBUG */
	if (TOACT_VAL > 1)
		fprintf (stderr,
		        "\nOCCHIO!!!\nTIMEOUT ATTIVITA' = %f\n\n",
		        TOACT_VAL);

	/*
	 * Inizializzazione moduli.
	 */
	init_timeout_module ();
	init_segment_module ();

	for (;;) {
		activate_channels ();

		min_timeout = check_timeouts ();

		/* Lo stato dei canali p_net e' controllato dalle funzioni. */
		if (channel_is_connected (HOSTCD)) {
			feed_upload ();
			feed_download ();
		}

		/*
		 * Select
		 */
		do {
			struct timeval *toptr;

			if (min_timeout > 0) {
				toptr = &tv_timeout;
				d2tv (min_timeout, toptr);
			} else
				toptr = NULL;

			/* Inizializzazione dei set. */
			FD_ZERO (&rdset);
			FD_ZERO (&wrset);

			/* Selezione dei fd in base allo stato dei canali. */
			maxfd = set_file_descriptors (&rdset, &wrset);

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
		if (rdy > 0 ) for (cd = 0; cd < CHANNELS; cd++) {
			fd_t listfd = channel_get_listfd (cd);
			fd_t sockfd = channel_get_sockfd (cd);

			/* Connessione da concludere. */
			if (channel_is_connecting (cd)
			    && FD_ISSET (sockfd, &wrset)) {
				err = finalize_connection (cd);
				if (err) {
					channel_close (cd);
				} else {
					channel_prepare_io (cd);
					printf ("Canale %s connesso.\n",
							channel_name (cd));
				}
			}

			/* Connessione da accettare. */
			else if (channel_is_listening (cd)
			         && FD_ISSET (listfd, &rdset)) {
				err = accept_connection (cd);
				if (err) {
					channel_close (cd);
				} else {
					channel_prepare_io (cd);
					printf ("Canale %s, connessione "
							"accettata.\n",
							channel_name (cd));
				}
			}

			/* I/O. */
			else {
				/* Dati da leggere. */
				if (channel_is_connected (cd)
				    && FD_ISSET (sockfd, &rdset)) {
					ssize_t nr;
					nr = channel_read (cd);
					if (errno != 0) {
						perror ("errore channel_read");
						channel_close (cd);
					}
				}

				/* Dati da scrivere. */
				if (channel_is_connected (cd)
				    && FD_ISSET (sockfd, &wrset)) {
					ssize_t nw;
					nw = channel_write (cd);
					if (errno != 0) {
						perror ("errore channel_write");
						channel_close (cd);
					}
				}
			}
		}
	}
	return 0;
}
