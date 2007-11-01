#include "h/core.h"
#include "h/channel.h"
#include "h/getargs.h"
#include "h/proxy.h"
#include "h/util.h"
#include "h/types.h"

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
static const char* defconnaddr[NETCHANNELS] = {
	"127.0.0.1",
	"127.0.0.1",
	"127.0.0.1"
};

/* Porte per connessioni al Ritardatore. */
static const port_t defconnport[NETCHANNELS] = {
	7001,
	7002,
	7003
};

/* Porta di ascolto per accettare la connessione del Sender. */
static const port_t deflistport = 6001;


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static bool activable_if_connected (void *arg);
static void print_help (const char *);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
main (int argc, char **argv) {

	int i;
	int err;
	struct proxy ps;

	/*
	 * Inizializzazioni con valori di default.
	 */

	proxy_init (&ps);

	/* Canali con il Ritardatore. */
	for (i = 0; i < NETCHANNELS; i++) {
		channel_init (&ps.p_net[i]);

		err = set_addr (&ps.p_net[i].c_raddr,
		                defconnaddr[i], defconnport[i]);
		assert (!err);

		/* Ogni canale con il Ritardatore va attivato solo dopo che
		 * l'host si e' connesso. */
		channel_set_activation_condition (&ps.p_net[i],
		                                  &activable_if_connected,
		                                  &ps.p_host);
	}

	/* Canale con il Sender. */
	channel_init (&ps.p_host);
	err = set_addr (&ps.p_host.c_laddr, NULL, deflistport);
	assert (!err);

	/*
	 * Personalizzazioni da riga di comando.
	 */

	err = getargs (argc, argv, "papapap",
	              &ps.p_host.c_laddr.sin_port,
		      &ps.p_net[0].c_raddr, &ps.p_net[0].c_raddr.sin_port,
		      &ps.p_net[1].c_raddr, &ps.p_net[1].c_raddr.sin_port,
		      &ps.p_net[2].c_raddr, &ps.p_net[2].c_raddr.sin_port);
	if (err) {
		print_help (argv[0]);
		exit (EXIT_FAILURE);
	}

	/* Stampa informazioni. */
	printf ("Canale con il Sender: %s\n", channel_name (&ps.p_host));
	for (i = 0; i < NETCHANNELS; i++) {
		printf ("Canale %d con il Ritardatore: %s\n",
		         i, channel_name (&ps.p_net[i]));
	}

	return core (&ps);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static bool
activable_if_connected (void *arg) {
	/* Condizione di connessione per i canali con il Ritardatore. */

	struct chan *ch;

	assert (arg != NULL);

	ch = (struct chan *)arg;

	if (channel_is_connected (ch)) {
		return TRUE;
	}
	return FALSE;
}


static void
print_help (const char *program_name) {
	printf ("%s [[[ porta_locale ] ip ] porta ] ...\n",
	        program_name);
	printf ("\n"
"Attende la connessione dal Sender su porta_locale e si connette al\n"
"Ritardatore, che deve essere in ascolto sugli indirizzi ip:porta. Se un\n"
"un argomento non viene specificato oppure è -, viene usato il valore\n"
"predefinito.\n"
		);
}
