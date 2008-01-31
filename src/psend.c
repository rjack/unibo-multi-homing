#include "h/core.h"
#include "h/channel.h"
#include "h/getargs.h"
#include "h/util.h"
#include "h/types.h"

#include <config.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


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

static int
get_psend_args (int argc, char **argv,
		port_t *hostlistport,
		char *netconnaddr[NETCHANNELS],
		port_t netconnport[NETCHANNELS]);
static void print_help (const char *);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
main (int argc, char **argv)
{
	int err;
	cd_t cd;

	err = get_psend_args (argc, argv,
			&hostlistport, netconnaddr, netconnport);
	if (err)
		goto error;

	err = proxy_init (hostlistport, netconnaddr, netconnport,
			NULL, NULL, 0);
	if (err)
		goto error;

	/* Stampa informazioni. */
	printf ("Canale con il Sender: %s\n", channel_name (HOSTCD));
	for (cd = NETCD; cd < NETCHANNELS; cd++) {
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
get_psend_args (int argc, char **argv,
		port_t *hostlistport,
		char *netconnaddr[NETCHANNELS],
		port_t netconnport[NETCHANNELS])
{
	/* TODO get_psend_args */
	return 0;
}


static void
print_help (const char *program_name)
{
	printf ("%s [[[ porta_locale ] ip ] porta ] ...\n",
	        program_name);
	printf ("\n"
"Attende la connessione dal Sender su porta_locale e si connette al\n"
"Ritardatore, che deve essere in ascolto sugli indirizzi ip:porta. Se un\n"
"un argomento non viene specificato oppure e' -, viene usato il valore\n"
"predefinito.\n"
		);
}
