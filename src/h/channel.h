/* channel.h - funzioni di gestione dei canali.
 *
 * Giacomo Ritucci, 24/09/2007 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
channel_init (struct chan *ch);
/* Inizializza i campi della struttura ch. */


void
channel_invalidate (struct chan *ch);
/* Rende il canale inutilizzabile. */


bool
channel_is_activable (struct chan *ch);


bool
channel_is_connected (struct chan *ch);


bool
channel_is_connecting (struct chan *ch);


bool
channel_is_listening (struct chan *ch);


char *
channel_name (struct chan *ch);
/* Ritorna una stringa allocata staticamente e terminata da '\0' della forma
 * "xxx.xxx.xxx.xxx:yyyyy - xxx.xxx.xxx.xxx:yyyyy", dove il primo è
 * l'indirizzo locale, il secondo quello remoto. */


void
channel_set_activation_condition
(struct chan *ch, bool (*funct)(void *), void *arg);


#endif /* CHANNEL_H */
