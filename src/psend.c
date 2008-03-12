#include "h/core.h"
#include "h/channel.h"
#include "h/segment.h"
#include "h/timeout.h"
#include "h/util.h"
#include "h/types.h"

#include <config.h>


/*******************************************************************************
			       Variabili locali
*******************************************************************************/

/*
 * Valori di default.
 */

/* Indirizzi per connessioni al Ritardatore. */
static char* netconnaddr[NETCHANNELS] = {
	"127.0.0.1",
	"127.0.0.1",
	"127.0.0.1"
};

/* Porte per connessioni al Ritardatore. */
static port_t netconnport[NETCHANNELS] = {
	7001,
	7002,
	7003
};

/* Porta di ascolto per accettare la connessione del Sender. */
static port_t hostlistport = 6001;


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int get_psend_args (int argc, char **argv, port_t *hostlistport, char
		*netconnaddr[NETCHANNELS], port_t netconnport[NETCHANNELS]);
static void print_help (const char *);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
main (int argc, char **argv)
{
	int err;
	cd_t cd;

	err = get_psend_args (argc, argv, &hostlistport, netconnaddr,
			netconnport);
	if (err)
		goto error;

	init_timeout_module ();
	init_segment_module ();

	err = proxy_init (hostlistport, netconnaddr, netconnport, NULL, NULL,
			0);
	if (err)
		goto error;

	/* Stampa informazioni. */
	printf ("Canale con il Sender: %s\n", channel_name (HOSTCD));
	for (cd = NETCD; cd < NETCD + NETCHANNELS; cd++) {
		printf ("Canale %d con il Ritardatore: %s\n", cd,
				channel_name (cd));
	}

	return core ();

error:
	print_help (argv[0]);
	exit (EXIT_FAILURE);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static int
get_psend_args (int argc, char **argv, port_t *hostlistport,
		char *netconnaddr[NETCHANNELS],
		port_t netconnport[NETCHANNELS])
{
	int i;
	int j;	/* Itera su argv. */
	char *errmsg;

	/* Errore e default. */
	if (argc != 1 && argc != 8) {
		errmsg = "Numero di argomenti errato.";
		goto error;
	}
	if (argc == 1)
		goto success;

	/* Porta di ascolto connessione Sender. */
	if (!streq (argv[1], "-")) {
		char *end;
		*hostlistport = strtol (argv[1], &end, 10);
		if (errno != 0 || argv[1] == end || *end!= '\0') {
			errmsg = "Porta non valida.";
			goto error;
		}
	}

	/* Porte e ip connessioni Ritardatore. */
	for (i = 0, j = 2; i < NETCHANNELS; i++, j++) {
		if (!streq (argv[j], "-"))
			netconnaddr[i] = argv[j];
		j++;
		if (!streq (argv[j], "-")) {
			char *end;
			netconnport[i] = strtol (argv[j], &end, 10);
			if (errno != 0 || argv[j] == end || *end!= '\0') {
				errmsg = "Porta non valida.";
				goto error;
			}
		}
	}


success:
	return 0;
error:
	fprintf (stderr, "%s\n", errmsg);
	return -1;
}


static void
print_help (const char *program_name)
{
	printf ("%s [ porta_locale ip porta ip porta ip porta ]\n",
			program_name);
	printf ("\n"
"Attende la connessione dal Sender su porta_locale e si connette al\n"
"Ritardatore, che deve essere in ascolto sugli indirizzi ip:porta.\n"
"Per utilizzare il valore predefinito di un parametro, specificare '-'\n");
}
