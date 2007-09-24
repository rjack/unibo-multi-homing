/* psend.c - proxy sender
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

/* Valori di default. */
static const char* defconnaddr[NETCHANNELS] = {
	"127.0.0.1",
	"127.0.0.1",
	"127.0.0.1"
};

static const port_t defconnport[NETCHANNELS] = {
	7001,
	7002,
	7003
};

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
	/* I primi tre sono canali con il Ritardatore, l'ultimo quello con il
	 * Sender. */
	struct chan chnl[CHANNELS];

	/* Inizializzazioni con valori di default. */
	for (i = 0; i < CHANNELS; i++) {
		channel_init (&chnl[i]);

		/* Canali con il Ritardatore. */
		if (i < CHANNELS - 1) {
			ok = set_addr (&chnl[i].c_raddr,
			               defconnaddr[i],
			               defconnport[i]);
			assert (ok == TRUE);
		}
		/* Canale con il Sender. */
		else {
			ok = set_addr (&chnl[i].c_laddr, NULL, deflistport);
			assert (ok == TRUE);
		}
	}

	/* Lettura opzioni da riga di comando. */
	ok = getargs (argc, argv, "papapap",
	              &chnl[0].c_laddr.sin_port,
		      &chnl[1].c_raddr, &chnl[1].c_raddr.sin_port,
		      &chnl[2].c_raddr, &chnl[2].c_raddr.sin_port,
		      &chnl[3].c_raddr, &chnl[3].c_raddr.sin_port);
	if (ok == FALSE) {
		print_help (argv[0]);
		exit (EXIT_FAILURE);
	}

	/* TODO Lancio del core. */

	return 0;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static void
print_help (const char *program_name) {
	printf ("%s [[[ porta_locale ] ip ] porta ] ...\n",
	        program_name);
	printf (
"Attende la connessione dal Sender su porta_locale e si connette al\n"
"Ritardatore, in ascolto sugli indirizzi ip:porta.\n"
"Se un argomento è -, viene usato il valore predefinito.\n"
	);
}
