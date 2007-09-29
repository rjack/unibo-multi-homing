#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <sys/select.h>


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

	for (i = 0; i < CHANNELS; i++) {
		if (!addr_is_set (&chnl[i].c_laddr)
		    && addr_is_set (&chnl[i].c_raddr)) {
			/* TODO
			 * - nuovo socket
			 * - connect non bloccante */
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
