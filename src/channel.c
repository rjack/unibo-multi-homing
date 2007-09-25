/* channel.c - funzioni di gestione dei canali.
 *
 * Giacomo Ritucci, 24/09/2007 */

#include "types.h"
#include "util.h"

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


char *
channel_name (struct chan *ch) {
	/* Ritorna una stringa allocata staticamente e terminata da '\0' della
	 * forma "xxx.xxx.xxx.xxx:yyyyy - xxx.xxx.xxx.xxx:yyyyy", dove il
	 * primo è l'indirizzo locale, il secondo quello remoto. */

	int i;
	char *bufptr;
	static char bufname[46];

	assert (ch != NULL);

	memset (bufname, 0, sizeof (bufname));

	/* Indrizzo locale. */
	bufptr = addrstr (&ch->c_laddr, bufname);

	/* Separatore. */
	strcpy (bufptr, " - ");

	bufptr = strchr (bufptr, '\0');
	assert (bufptr != NULL);

	/* Indirizzo remoto. */
	bufptr = addrstr (&ch->c_raddr, bufptr);

	/* Overflow? */
	assert (bufname[45] == '\0');

	return bufname;
}
