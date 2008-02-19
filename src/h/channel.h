#ifndef CHANNEL_H
#define CHANNEL_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

bool
channel_can_read (cd_t cd);


bool
channel_can_write (cd_t cd);


fd_t
channel_get_listfd (cd_t cd);


fd_t
channel_get_sockfd (cd_t cd);


int
channel_init (cd_t cd, port_t listport, char *connip, port_t connport);


void
channel_invalidate (cd_t cd);
/* Rende il canale inutilizzabile. */


bool
channel_is_activable (cd_t cd);


bool
channel_is_connected (cd_t cd);


bool
channel_is_connecting (cd_t cd);


bool
channel_is_listening (cd_t cd);


bool
channel_must_connect (cd_t cd);


bool
channel_must_listen (cd_t cd);


char *
channel_name (cd_t cd);
/* Ritorna una stringa allocata staticamente e terminata da '\0' della forma
 * "xxx.xxx.xxx.xxx:yyyyy - xxx.xxx.xxx.xxx:yyyyy", dove il primo e'
 * l'indirizzo locale, il secondo quello remoto. */


void
channel_prepare_io (cd_t cd);


int
channel_read (cd_t cd);


int
channel_write (cd_t cd);


void
feed_upload (void);


int
proxy_init (port_t hostlistport,
		char *netconnaddr[NETCHANNELS],
		port_t netconnport[NETCHANNELS],
		port_t netlistport[NETCHANNELS],
		char *hostconnaddr, port_t hostconnport);

int
accept_connection (cd_t cd);


void
activate_channels (void);


int
finalize_connection (cd_t cd);


fd_t
set_file_descriptors (fd_set *rdset, fd_set *wrset);


void
set_net_upload_reorg (void);

#endif /* CHANNEL_H */
