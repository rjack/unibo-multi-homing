/* getargs.c - lettura argomenti da linea di comando.
 *
 * Giacomo Ritucci, 23/09/2007 */

#include "util.h"
#include "types.h"

#include <assert.h>
#include <errno.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


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
		port_t *port;

		switch (fmt[i]) {

		/* Indirizzo ip. */
		case 'a' :
			addr = va_arg (args, struct sockaddr_in *);
			if (!streq (argv[j], "-")) {
				if (!inet_aton (argv[j], &addr->sin_addr)) {
					fprintf (stderr,
					         "ip non valido: %s.\n",
						 argv[j]);
					ok = FALSE;
				}
			}
		break;

		/* Porta. */
		case 'p' :
			port = va_arg (args, port_t *);
			if (!streq (argv[j], "-")) {
				errno = 0;
				*port = strtol (argv[j], NULL, 10);
				if (errno != 0) {
					fprintf (stderr,
						 "porta non valida: %s.\n",
						 argv[j]);
					ok = FALSE;
				} else {
					*port = htons (*port);
				}
			}
		break;

		/* Errore. */
		default :
			fprintf (stderr, "formato non riconosciuto: %c.\n",
			         fmt[i]);
			ok = FALSE;
		}
	}

	va_end (args);

	return ok;
}
