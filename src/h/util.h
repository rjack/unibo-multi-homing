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

char *
addrstr (struct sockaddr_in *addr, char *buf);


bool
addr_is_set (struct sockaddr_in *addr);


bool
streq (const char *str1, const char *str2);


bool
set_addr (struct sockaddr_in *addr, const char *ip, port_t port);


#endif /* UTIL_H */
