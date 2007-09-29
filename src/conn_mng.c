#include "h/types.h"
#include "h/channel.h"
#include "h/util.h"

#include <assert.h>
#include <errno.h>
#include <sys/select.h>
#include <stdio.h>


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

bool connect_noblock (struct chan *ch);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
manage_connections (struct chan* chnl) {
	/* Attiva tutti i canali che hanno una struct sockaddr_in impostata e
	 * una ancora inizializzata.
	 * I canali con entrambe le struct impostate sono considerati gia'
	 * connessi; quelli con entrambe le struct inizializzate non
	 * piu' utilizzabili.
	 *
	 * Se e' impostata la struct locale, viene creato un socket listening
	 * e iniziata una accept non bloccante; altrimenti viene creato un
	 * socket e iniziata una connect non bloccante. */

	int i;
	int err;

	for (i = 0; i < CHANNELS; i++) {
		if (!addr_is_set (&chnl[i].c_laddr)
		    && addr_is_set (&chnl[i].c_raddr)) {
			err = connect_noblock (&chnl[i]);
			/* TODO if err... */
		}
		if (addr_is_set (&chnl[i].c_laddr)
		    && !addr_is_set (&chnl[i].c_raddr)) {
			/* TODO
			 * - nuovo socket listening
			 * - accept non bloccante */
		}
	}
}


fd_t
set_file_descriptors (struct chan *chnl, fd_set *rdset, fd_set *wrset) {
	/* TODO */

	return -1;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

bool
connect_noblock (struct chan *ch) {
	/* Connessione non bloccante. Crea un socket, lo imposta non bloccante
	 * ed esegue una connect (senza bind) usando la struct sockaddr_in del
	 * canale.
	 *
	 * Ritorna TRUE se la connect riesce subito o e' in corso, FALSE se
	 * fallisce. */

	int err;

	assert (ch != NULL);
	assert (addr_is_set (&ch->c_raddr));
	assert (ch->c_sockfd < 0);

	/* Nuovo socket tcp. */
	ch->c_sockfd = xtcp_socket();

	/* Non bloccante, se fallisce RITORNA subito. */
	if (!tcp_set_block (ch->c_sockfd, FALSE)) {
		fprintf (stderr,
		         "Canale %s, impossibile disabilitare Nagle: %s\n",
		         channel_name (ch), strerror (errno));
		close (ch->c_sockfd);
		ch->c_sockfd = -1;
		return FALSE;
	}

	/* TODO se necessario modificare i buffer TCP qui. */

	do {
		err = connect (ch->c_sockfd,
	                      (struct sockaddr *)&ch->c_raddr,
	                      sizeof (ch->c_raddr));
	} while (err == -1 && errno == EINTR);

	/* Se riuscita subito o in corso, ritorna TRUE; altrimenti FALSE. */
	if (err == 0 || errno == EINPROGRESS) {
		printf ("Canale %s %s.\n", channel_name (ch),
		        (err == 0) ? "connesso" : "in connessione");
		return TRUE;
	} else {
		fprintf (stderr, "Canale %s, errore di connessione: %s\n",
		         channel_name (ch), strerror (errno));
		return FALSE;
	}
}
