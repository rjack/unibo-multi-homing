/* util.c - funzioni di utilità generale.
 *
 * Giacomo Ritucci, 23/09/2007 */

#include "h/util.h"
#include "h/types.h"

#include <assert.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <string.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

bool
addr_is_set (struct sockaddr_in *addr) {
	/* Per controllare che sia stata impostata e' sufficiente controllare
	 * sin_family. */

	if (addr->sin_family == AF_INET) {
		return TRUE;
	}
	return FALSE;
}


char *
addrstr (struct sockaddr_in *addr, char *buf) {
	/* Copia la stringa in formato xxx.xxx.xxx.xxx:yyyyy nel buffer buf,
	 * che deve essere grande a sufficienza. */

	char *name;

	/* Copia dell'indirizzo ip. */
	name = (char *) inet_ntop (AF_INET, &addr->sin_addr, buf, INET_ADDRSTRLEN);
	assert (name != NULL);

	/* Copia del numero di porta. */
	name = strchr (name, '\0');
	sprintf (name, ":%d", ntohs (addr->sin_port));
	/* Posiziona name alla fine della stringa. */
	name = strchr (name, '\0');
	/* Overflow? */
	assert (name < (buf + INET_ADDRSTRLEN + 1 + 5));

	return name;
}


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
