#include "h/channel.h"
#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <string.h>


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static bool always (void *discard);
static bool never (void *discard);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

bool
channel_can_read (struct chan *ch) {
	/* Chiama la funzione associata a ch->c_can_read, che ritorna TRUE se
	 * ch e' pronto per leggere da ch->c_sockfd. */

	assert (ch != NULL);
	assert (ch->c_can_read != NULL);

	return ch->c_can_read (ch->c_can_read_arg);
}


bool
channel_can_write (struct chan *ch) {
	/* Chiama la funzione associata a ch->c_can_write, che ritorna TRUE se
	 * ch e' pronto per scrivere su ch->c_sockfd. */

	assert (ch != NULL);
	assert (ch->c_can_write != NULL);

	return ch->c_can_write (ch->c_can_write_arg);
}


void
channel_init (struct chan *ch) {
	assert (ch != NULL);

	ch->c_sockfd = -1;
	ch->c_listfd = -1;

	memset (&ch->c_laddr, 0, sizeof (ch->c_laddr));
	memset (&ch->c_raddr, 0, sizeof (ch->c_raddr));

	ch->c_rcvbuf_len = 0;
	ch->c_sndbuf_len = 0;

	channel_set_condition (ch, SET_ACTIVABLE, &always, NULL);
	channel_set_condition (ch, SET_CAN_READ, &never, NULL);
	channel_set_condition (ch, SET_CAN_WRITE, &never, NULL);
}


void
channel_invalidate (struct chan *ch) {
	channel_init (ch);
	channel_set_condition (ch, SET_ACTIVABLE, NULL, NULL);
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

	assert (ch != NULL);

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

	assert (ch != NULL);

	if (ch->c_listfd >= 0
	    && addr_is_set (&ch->c_laddr)
	    && !addr_is_set (&ch->c_raddr)) {
		assert (ch->c_sockfd < 0);
		return TRUE;
	}
	return FALSE;
}


bool
channel_must_connect (struct chan *ch) {
	/* Usata da manage_connections per sapere se il canale deve essere
	 * connesso. */

	assert (ch != NULL);

	if (!addr_is_set (&ch->c_laddr)
	    && addr_is_set (&ch->c_raddr)
	    && ch->c_sockfd < 0) {
		assert (ch->c_listfd < 0);
		return TRUE;
	}
	return FALSE;
}


bool
channel_must_listen (struct chan *ch) {
	/* Usata da manage_connections per sapere se il canale deve essere
	 * messo in ascolto di connessioni. */

	assert (ch != NULL);

	if (addr_is_set (&ch->c_laddr)
	    && !addr_is_set (&ch->c_raddr)
	    && ch->c_listfd < 0) {
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
channel_set_condition
(struct chan *ch, enum set_condition cond, bool (*funct)(void *), void *arg) {
	/* Imposta la funzione e relativo argomento per verificare se ch e'
	 * nella condizione richiesta. Se funct e' NULL la risposta sara'
	 * sempre negativa. */

	assert (ch != NULL);
	assert (funct != NULL || arg == NULL);

	switch (cond) {
	case SET_ACTIVABLE:
		ch->c_is_activable = (funct == NULL ? &never : funct);
		ch->c_activable_arg = arg;
		break;
	case SET_CAN_READ:
		ch->c_can_read = (funct == NULL ? &never : funct);
		ch->c_can_read_arg = arg;
		break;
	case SET_CAN_WRITE:
		ch->c_can_write = (funct == NULL ? &never : funct);
		ch->c_can_write_arg = arg;
		break;
	default:
		assert (FALSE);
	}
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static bool
always (void *discard) {
	return TRUE;
}


static bool
never (void *discard) {
	return FALSE;
}
