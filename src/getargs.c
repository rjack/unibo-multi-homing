#include "h/util.h"

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

int
getargs (int argc, char **argv, char *fmt, ...) {

	int err = 0;
	int i;	/* su fmt */
	int j;	/* su argv */
	va_list args;

	assert (argv != NULL);
	assert (fmt != NULL);

	va_start (args, fmt);

	for (i = 0, j = 1;
	     !err && i <= strlen (fmt) && j < argc;
	     i++, j++) {
		struct sockaddr_in *addr;
		port_t *port;

		switch (fmt[i]) {

		/* Indirizzo ip. */
		case 'a' :
			addr = va_arg (args, struct sockaddr_in *);
			if (!streq (argv[j], "-")) {
				err = inet_pton (AF_INET, argv[j],
				                 &addr->sin_addr);
				assert (err >= 0);
				if (err == 0) {
					fprintf (stderr,
					         "ip non valido: %s.\n",
						 argv[j]);
					err = -1;
				} else {
					err = 0;
				}
			}
		break;

		/* Porta. */
		case 'p' :
			port = va_arg (args, port_t *);
			if (!streq (argv[j], "-")) {
				char *endptr;
				errno = 0;
				*port = strtol (argv[j], &endptr, 10);
				if (errno != 0
				    || argv[j] == endptr
				    || *endptr != '\0') {
					fprintf (stderr,
						 "porta non valida: %s.\n",
						 argv[j]);
					err = -1;
				} else {
					*port = htons (*port);
				}
			}
		break;

		/* Errore. */
		default :
			fprintf (stderr, "formato non riconosciuto: %c.\n",
			         fmt[i]);
			err = -1;
		}
	}

	va_end (args);

	return err;
}
