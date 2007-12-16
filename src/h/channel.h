#ifndef CHANNEL_H
#define CHANNEL_H

#include "types.h"


/*******************************************************************************
				     Enum
*******************************************************************************/

enum set_condition {
	SET_ACTIVABLE,
	SET_CAN_READ,
	SET_CAN_WRITE
};


/*******************************************************************************
				  Prototipi
*******************************************************************************/

bool
channel_can_read (struct chan *ch);


bool
channel_can_write (struct chan *ch);


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


bool
channel_must_connect (struct chan *ch);


bool
channel_must_listen (struct chan *ch);


char *
channel_name (struct chan *ch);
/* Ritorna una stringa allocata staticamente e terminata da '\0' della forma
 * "xxx.xxx.xxx.xxx:yyyyy - xxx.xxx.xxx.xxx:yyyyy", dove il primo è
 * l'indirizzo locale, il secondo quello remoto. */


int
channel_read (struct chan *ch);


void
channel_set_condition
(struct chan *ch, enum set_condition, condition_checker_t fun, void *arg);


int
channel_write (struct chan *ch);

#endif /* CHANNEL_H */
