/* channel.c - funzioni di gestione dei canali.
 *
 * Giacomo Ritucci, 24/09/2007 */

#include "h/types.h"
#include "h/util.h"

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


bool
channel_is_connecting (struct chan *ch) {
	/* Se il socket del canale e' valido ed e' impostato solo l'addr
	 * remoto e non quello locale, allora c'e' una connect non bloccante
	 * in corso. */

	if (ch->c_sockfd >= 0
	    && !addr_is_set (&ch->c_laddr)
	    && addr_is_set (&ch->c_raddr)) {
		assert (ch->c_listfd < 0);
		return TRUE;
	}
	return FALSE;
}


bool
channel_is_listening (struct chan *ch) {
	/* Se il socket listening del canale e' valido ed e' impostato l'addr
	 * locale ma non quello remoto, allora il canale e' in ascolto. */

	if (ch->c_listfd >= 0
	    && addr_is_set (&ch->c_laddr)
	    && !addr_is_set (&ch->c_raddr)) {
		assert (ch->c_sockfd < 0);
		return TRUE;
	}
	return FALSE;
}


char *
channel_name (struct chan *ch) {
	/* Ritorna una stringa allocata staticamente e terminata da '\0' della
	 * forma "xxx.xxx.xxx.xxx:yyyyy - xxx.xxx.xxx.xxx:yyyyy", dove il
	 * primo è l'indirizzo locale, il secondo quello remoto. */

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
