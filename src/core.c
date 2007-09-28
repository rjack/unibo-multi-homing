#include "h/types.h"

#include <assert.h>
#include <errno.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
core (struct chan *chnl) {
	bool fatal = FALSE;

	int rdy;
	fd_t maxfd;
	fd_set rdset;
	fd_set wrset;

	while (!fatal) {
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
			fatal = TRUE;
		} else {
			/* TODO gestione eventi. */
			assert (FALSE);
		}
	}
}
