#include "h/types.h"

#include <assert.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
proxy_init (struct proxy *px) {
	int i;

	assert (px != NULL);

	/* Collegamento puntatori.
	 * L'array p_chptr viene usato dalle funzioni che devono ciclare su
	 * tutti i canali senza distinzione di tipo. */
	for (i = 0; i < NETCHANNELS; i++) {
		px->p_chptr[i] = px->p_net[i];
	}
	px->p_chptr[i] = &px->p_host;
}
