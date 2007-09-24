/* channel.c - funzioni di gestione dei canali.
 *
 * Giacomo Ritucci, 24/09/2007 */

#include "types.h"

#include <assert.h>
#include <string.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
channel_init (struct chan *ch) {
	assert (ch != NULL);

	ch->c_sockfd = -1;
	ch->c_listfd = -1;

	memset (&ch->c_laddr, 0, sizeof (ch->c_laddr));
	memset (&ch->c_raddr, 0, sizeof (ch->c_raddr));
}
