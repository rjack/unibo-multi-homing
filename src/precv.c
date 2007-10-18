/* precv.c - proxy receiver
 *
 * Giacomo Ritucci, 22/09/2007 */

#include "h/core.h"
#include "h/channel.h"
#include "h/getargs.h"
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
	struct chan chnl[CHANNELS];

	/*
	 * Inizializzazioni con valori di default.
	 */

	/* Canali con il Ritardatore. */
	for (i = NET; i < NETCHANNELS; i++) {
		channel_init (&chnl[i]);

		err = set_addr (&chnl[i].c_laddr, NULL, deflistport[i]);
		assert (!err);
	}

	/* Canale con il Receiver. */
	channel_init (&chnl[HOST]);
	err = set_addr (&chnl[HOST].c_raddr, defconnaddr, defconnport);
	assert (!err);

	/* Il canale con il Receiver e' attivabile quando e' connesso almeno
	 * un canale con il Ritardatore. */
	channel_set_activation_condition (&chnl[i],
	                                  &activable_if_almost_one_connected,
	                                  &chnl[NET]);

	/*
	 * Personalizzazioni da riga di comando.
	 */
	err = getargs (argc, argv, "pppap",
	              &chnl[NET].c_laddr.sin_port,
	              &chnl[NET+1].c_laddr.sin_port,
	              &chnl[NET+2].c_laddr.sin_port,
		      &chnl[HOST].c_raddr, &chnl[HOST].c_raddr.sin_port);
	if (err) {
		print_help (argv[0]);
		exit (EXIT_FAILURE);
	}

	/* Stampa informazioni. */
	for (i = NET; i < NETCHANNELS; i++) {
		printf ("Canale %d con il Ritardatore: %s\n",
		         i - NET, channel_name (&chnl[i]));
	}
	printf ("Canale con il Receiver: %s\n", channel_name (&chnl[HOST]));

	return core (chnl);
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
