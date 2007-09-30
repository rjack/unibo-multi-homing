/* util.h - funzioni e macro di utilità generale.
 *
 * Giacomo Ritucci, 23/09/2007 */

#ifndef UTIL_H
#define UTIL_H


#include "types.h"

#include <netinet/ip.h>


/*******************************************************************************
				  Prototipi
*******************************************************************************/

/*
 * Funzioni su struct sockaddr_in.
 */

bool
addr_is_set (struct sockaddr_in *addr);


char *
addrstr (struct sockaddr_in *addr, char *buf);


bool
set_addr (struct sockaddr_in *addr, const char *ip, port_t port);


/*
 * Funzioni su stringhe.
 */

bool
streq (const char *str1, const char *str2);


/*
 * Funzioni per la gestione della memoria.
 */

void
xfree (void *ptr);


void *
xmalloc (size_t size);


/* 
 * Funzioni su socket e operazioni di rete.
 */

void
tcp_close (fd_t *fd);


bool
tcp_set_block (fd_t fd, bool must_block);


bool
tcp_set_nagle (fd_t fd, bool active);


void
tcp_sockname (fd_t fd, struct sockaddr_in *laddr);


fd_t
xtcp_socket (void);


#endif /* UTIL_H */
