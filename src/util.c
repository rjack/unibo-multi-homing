/* util.c - funzioni di utilità generale.
 *
 * Giacomo Ritucci, 23/09/2007 */

#define _GNU_SOURCE	/* per inet_aton */

#include "util.h"
#include "types.h"

#include <assert.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <string.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

bool
streq (const char *str1, const char *str2) {
	int cmp;

	assert (str1 != NULL);
	assert (str2 != NULL);

	cmp = strcmp (str1, str2);
	
	if (cmp == 0) return TRUE;
	return FALSE;
}


bool
set_addr (struct sockaddr_in *addr, const char *ip, port_t port) {
	assert (addr != NULL);

	memset (addr, 0, sizeof (struct sockaddr_in));
	addr->sin_family = AF_INET;

	if (ip == NULL) {
		addr->sin_addr.s_addr = htonl (INADDR_ANY);
	} else if (!inet_aton (ip, &addr->sin_addr)) {
		/* La stringa ip non ha un formato valido. */
		return FALSE;
	}
	addr->sin_port = htons (port);

	return TRUE;
}
