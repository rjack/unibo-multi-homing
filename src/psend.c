/* psend.c - proxy sender
 *
 * Giacomo Ritucci, 22/09/2007 */

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

		ok = set_addr (&chnl[i].c_raddr, defconnaddr[i],
		               defconnport[i]);
		assert (ok == TRUE);
	}

	/* Canale con il Sender. */
	channel_init (&chnl[HOST]);
	ok = set_addr (&chnl[HOST].c_laddr, NULL, deflistport);
	assert (ok == TRUE);

	/*
	 * Personalizzazioni da riga di comando.
	 */

	ok = getargs (argc, argv, "papapap",
	              &chnl[HOST].c_laddr.sin_port,
		      &chnl[NET].c_raddr, &chnl[NET].c_raddr.sin_port,
		      &chnl[NET+1].c_raddr, &chnl[NET+1].c_raddr.sin_port,
		      &chnl[NET+2].c_raddr, &chnl[NET+2].c_raddr.sin_port);
	if (ok == FALSE) {
		print_help (argv[0]);
		exit (EXIT_FAILURE);
	}

	/* Stampa informazioni. */
	printf ("Canale il Sender: %s\n", channel_name (&chnl[HOST]));
	for (i = NET; i < NETCHANNELS; i++) {
		printf ("Canale %d con il Ritardatore: %s\n",
		         i - NET, channel_name (&chnl[i]));
	}

	return core (chnl);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

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
