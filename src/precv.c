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

/* Porte di ascolto per accettare connessioni da Ritardatore. */
static const port_t deflistport[NETCHANNELS] = {
	8001,
	8002,
	8003
};

/* Indirizzo e porta di connessione al Receiver. */
static const char* defconnaddr = "127.0.0.1";
static const port_t defconnport = 9001;


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static bool activable_if_almost_one_connected (void *arg);
static void print_help (const char *);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
main (int argc, char **argv) {

	int i;
	int err;
	struct proxy pr;

	/*
	 * Inizializzazioni con valori di default.
	 */
	proxy_init (&pr);

	/* Canali con il Ritardatore:
	 * - porte di ascolto
	 * - buffer tcp di invio. */
	for (i = 0; i < NETCHANNELS; i++) {
		channel_init (&pr.p_net[i]);

		/* Piu' piccolo possibile per notare prima congestioni e
		 * blocchi. */
		pr.p_net[i].c_sndbuf_len = 1024;

		err = set_addr (&pr.p_net[i].c_laddr, NULL, deflistport[i]);
		assert (!err);
	}

	/* Canale con il Receiver: indirizzi remoti e condizione di
	 * attivazione. */
	channel_init (&pr.p_host);
	err = set_addr (&pr.p_host.c_raddr, defconnaddr, defconnport);
	assert (!err);

	channel_set_condition (&pr.p_host, SET_ACTIVABLE,
			       &activable_if_almost_one_connected, pr.p_net);

	/*
	 * Impostazioni da riga di comando.
	 */
	err = getargs (argc, argv, "pppap",
	              &pr.p_net[0].c_laddr.sin_port,
	              &pr.p_net[1].c_laddr.sin_port,
	              &pr.p_net[2].c_laddr.sin_port,
		      &pr.p_host.c_raddr, &pr.p_host.c_raddr.sin_port);
	if (err) {
		print_help (argv[0]);
		exit (EXIT_FAILURE);
	}

	/* Stampa informazioni. */
	for (i = 0; i < NETCHANNELS; i++) {
		printf ("Canale %d con il Ritardatore: %s\n",
		         i, channel_name (&pr.p_net[i]));
	}
	printf ("Canale con il Receiver: %s\n", channel_name (&pr.p_host));

	return core (&pr);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static bool
activable_if_almost_one_connected (void *arg) {
	int i;
	bool ok;
	struct chan *ch;

	assert (arg != NULL);

	ch = (struct chan *)arg;

	for (i = 0, ok = FALSE;
	     i < NETCHANNELS && !ok;
	     i++) {
		ok = channel_is_connected (&ch[i]);
	}

	return ok;
}


static void
print_help (const char *program_name) {
	printf (
"%s [[[[[ porta_locale ] porta_locale ] porta_locale ] ip ] porta ]\n",
	        program_name);
	printf ("\n"
"Attende una connessione dal Ritardatore su una delle porte locali e si\n"
"connette al Receiver, che deve essere in ascolto sull'indirizzo ip:porta. Se\n"
"un argomento non viene specificato oppure è -, viene usato il valore\n"
"predefinito.\n"
		);
}
