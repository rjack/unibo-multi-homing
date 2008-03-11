#include "h/core.h"
#include "h/channel.h"
#include "h/getargs.h"
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

/* Porte di ascolto per accettare connessioni da Ritardatore. */
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

static int
get_precv_args (int argc, char **argv, port_t netlistport[NETCHANNELS],
		char **hostconnaddr, port_t *hostconnport);
static void print_help (const char *);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
main (int argc, char **argv)
{
	int err;
	cd_t cd;

	err = get_precv_args (argc, argv,
			netlistport, &hostconnaddr, &hostconnport);
	if (err)
		goto error;

	err = proxy_init (0, NULL, NULL,
			netlistport, hostconnaddr, hostconnport);
	if (err)
		goto error;

	/* Stampa informazioni. */
	for (cd = NETCD; cd < NETCHANNELS; cd++) {
		printf ("Canale %d con il Ritardatore: %s\n",
		         cd, channel_name (cd));
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
	/* TODO get_precv_args */
	return 0;
}


static void
print_help (const char *program_name)
{
	printf (
"%s [[[[[ porta_locale ] porta_locale ] porta_locale ] ip ] porta ]\n",
	        program_name);
	printf ("\n"
"Attende una connessione dal Ritardatore su una delle porte locali e si\n"
"connette al Receiver, che deve essere in ascolto sull'indirizzo ip:porta. Se\n"
"un argomento non viene specificato oppure e' -, viene usato il valore\n"
"predefinito.\n"
		);
}
