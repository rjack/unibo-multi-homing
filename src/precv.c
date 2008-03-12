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

/* Porte di ascolto per connessioni dal Ritardatore. */
static port_t netlistport[NETCHANNELS] = {
	8001,
	8002,
	8003
};

/* Indirizzo e porta di connessione al Receiver. */
static char* hostconnaddr = "127.0.0.1";
static port_t hostconnport = 9001;


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int get_precv_args (int argc, char **argv,
		port_t netlistport[NETCHANNELS], char **hostconnaddr,
		port_t *hostconnport);
static void print_help (const char *);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
main (int argc, char **argv)
{
	int err;
	cd_t cd;

	err = get_precv_args (argc, argv, netlistport, &hostconnaddr,
			&hostconnport);
	if (err)
		goto error;

	enable_ack_timeout ();
	init_timeout_module ();
	init_segment_module ();

	err = proxy_init (0, NULL, NULL, netlistport, hostconnaddr,
			hostconnport);
	if (err)
		goto error;

	/* Stampa informazioni. */
	for (cd = NETCD; cd < NETCD + NETCHANNELS; cd++) {
		printf ("Canale %d con il Ritardatore: %s\n", cd,
				channel_name (cd));
	}
	printf ("Canale con il Receiver: %s\n", channel_name (HOSTCD));

	return core ();

error:
	print_help (argv[0]);
	exit (EXIT_FAILURE);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static int
get_precv_args (int argc, char **argv, port_t netlistport[NETCHANNELS],
		char **hostconnaddr, port_t *hostconnport)
{
	int i;
	int j;	/* Itera su argv. */
	char *errmsg;

	/* Errore e default. */
	if (argc != 1 && argc != 6) {
		errmsg = "Numero di argomenti errato.";
		goto error;
	}
	if (argc == 1)
		goto success;

	/* Porte di ascolto connessioni Ritardatore. */
	for (i = 0, j = 1; i < NETCHANNELS; i++, j++)
		if (!streq (argv[j], "-")) {
			char *end;
			netlistport[i] = strtol (argv[j], &end, 10);
			if (errno != 0 || argv[j] == end || *end!= '\0') {
				errmsg = "Porta non valida.";
				goto error;
			}
		}

	/* Indirizzo connessione Receiver. */
	if (!streq (argv[j], "-"))
		*hostconnaddr = argv[j];
	j++;
	if (!streq (argv[j], "-")) {
		char *end;
		*hostconnport = strtol (argv[j], &end, 10);
		if (errno != 0 || argv[j] == end || *end!= '\0') {
			errmsg = "Porta non valida.";
			goto error;
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
	printf (
"%s [ porta_locale  porta_locale  porta_locale ip  porta ]\n",
	        program_name);
	printf ("\n"
"Attende una connessione dal Ritardatore su ognuna dell porte locali e si\n"
"connette al Receiver, che deve essere in ascolto sull'indirizzo ip:porta.\n"
"Per utilizzare il valore predefinito di un parametro, specificare '-'\n");
}
