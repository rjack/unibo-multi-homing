/* getargs.c - lettura argomenti da linea di comando.
 *
 * Giacomo Ritucci, 23/09/2007 */

#include "util.h"
#include "types.h"

#include <assert.h>
#include <netinet/ip.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

bool
getargs (int argc, char **argv, char *fmt, ...) {
	/* Secondo il formato fmt, converte i vari argv[i] in indirizzi o
	 * porte e li scrive negli indirizzi dati.
	 *
	 * Ogni carattere della stringa fmt specifica o un indirizzo ip ('a')
	 * o una porta ('p').
	 *
	 * Se il valore da linea di comando è '-', lascia il valore di default.
	 *
	 * Ritorna TRUE se riesce, FALSE se fallisce. */

	bool ok = TRUE;
	int i;	/* su fmt */
	int j;	/* su argv */
	va_list args;

	assert (argv != NULL);
	assert (fmt != NULL);

	va_start (args, fmt);

	for (i = 0, j = 1;
	     ok == TRUE && i <= strlen (fmt) && j < argc;
	     i++, j++) {
		struct sockaddr_in *addr;
		uint32_t *port;

		switch (fmt[i]) {

		case 'a' :
			addr = va_arg (args, struct sockaddr_in *);
			if (!streq (argv[j], "-")) {
				/* TODO inet_aton argv[j] -> *addr */
				/* TODO if (error) ok = FALSE */
			}
			break;

		case 'p' :
			port = va_arg (args, uint32_t *);
			if (!streq (argv[j], "-")) {
				*port = atoi (argv[j]);
				/* TODO if (error) ok = FALSE */
			}
			break;

		default :
			/* TODO error */
			ok = FALSE;
		}
	}

	va_end (args);

	return ok;
}
