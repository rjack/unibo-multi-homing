/* channel.c - funzioni di gestione dei canali.
 *
 * Giacomo Ritucci, 24/09/2007 */

#include "h/channel.h"
#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <string.h>


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static bool always_activable (void *discard);
static bool never_activable (void *discard);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

bool
channel_can_read (struct chan *ch) {
	assert (ch != NULL);

	/* TODO */

	return FALSE;
}


bool
channel_can_write (struct chan *ch) {
	assert (ch != NULL);

	/* TODO */

	return FALSE;
}


void
channel_init (struct chan *ch) {
	assert (ch != NULL);

	ch->c_sockfd = -1;
	ch->c_listfd = -1;

	memset (&ch->c_laddr, 0, sizeof (ch->c_laddr));
	memset (&ch->c_raddr, 0, sizeof (ch->c_raddr));

	channel_set_activation_condition (ch, &always_activable, NULL);
}


void
channel_invalidate (struct chan *ch) {
	channel_init (ch);
	channel_set_activation_condition (ch, NULL, NULL);
}


bool
channel_is_activable (struct chan *ch) {
	/* Chiama la funzione associata a ch che decide se il canale sia
	 * attivabile o meno. */

	assert (ch != NULL);
	assert (ch->c_is_activable != NULL);

	return ch->c_is_activable (ch->c_activable_arg);
}


bool
channel_is_connected (struct chan *ch) {
	/* Il canale e' connesso quando entrambi gli addr sono impostati, il
	 * socket listening e' chiuso e il socket e' connesso. */

	assert (ch != NULL);

	if (ch->c_listfd < 0
	    && ch->c_sockfd >= 0
	    && addr_is_set (&ch->c_laddr)
	    && addr_is_set (&ch->c_raddr)) {
		return TRUE;
	}
	return FALSE;
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


void
channel_set_activation_condition
(struct chan *ch, bool (*funct)(void *), void *arg) {
	/* Imposta la funzione e relativo argomento per verificare la
	 * condizione di attivazione di ch. Se funct e' NULL, il canale non
	 * verra' mai attivato. */

	assert (ch != NULL);
	assert (funct != NULL || arg == NULL);
	
	ch->c_is_activable = (funct == NULL ?
	                      &never_activable : funct);
	ch->c_activable_arg = arg;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static bool
always_activable (void *discard) {
	return TRUE;
}


static bool
never_activable (void *discard) {
	return FALSE;
}
