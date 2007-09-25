/* precv.c - proxy receiver
 *
 * Giacomo Ritucci, 22/09/2007 */

#include "util.h"
#include "types.h"

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

static void print_help (const char *);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
main (int argc, char **argv) {

	int i;
	bool ok;
	struct chan chnl[CHANNELS];

	/*
	 * Inizializzazioni con valori di default.
	 */

	/* Canali con il Ritardatore. */
	for (i = NET; i < NETCHANNELS; i++) {
		channel_init (&chnl[i]);

		ok = set_addr (&chnl[i].c_laddr, NULL, deflistport[i]);
		assert (ok == TRUE);
	}

	/* Canale con il Receiver. */
	channel_init (&chnl[HOST]);
	ok = set_addr (&chnl[HOST].c_raddr, defconnaddr, defconnport);
	assert (ok == TRUE);

	/*
	 * Personalizzazioni da riga di comando.
	 */
	ok = getargs (argc, argv, "pppap",
	              &chnl[NET].c_laddr.sin_port,
	              &chnl[NET+1].c_laddr.sin_port,
	              &chnl[NET+2].c_laddr.sin_port,
		      &chnl[HOST].c_raddr, &chnl[HOST].c_raddr.sin_port);
	if (ok == FALSE) {
		print_help (argv[0]);
		exit (EXIT_FAILURE);
	}

	/* Stampa informazioni. */
	for (i = NET; i < NETCHANNELS; i++) {
		printf ("Canale %d con il Ritardatore: %s\n",
		         i - NET, channel_name (&chnl[i]));
	}
	printf ("Canale il Receiver: %s\n", channel_name (&chnl[HOST]));

	/* TODO lancio del core. */

	return 0;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

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
