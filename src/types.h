/* types.h - definizioni dei tipi per tutto il progetto.
 *
 * Giacomo Ritucci, 23/09/2007 */

#ifndef MH_TYPES_H
#define MH_TYPES_H

#include <arpa/inet.h>


/*******************************************************************************
				   Costanti
*******************************************************************************/

/* Numero di canali di rete. */
#define     NETCHANNELS    3
/* Numero di canali totali. */
#define     CHANNELS       (NETCHANNELS + 1)
/* Id del canale dell'host: nell'array viene subito dopo a quelli di rete. */
#define     HOST           (NETCHANNELS)


/*******************************************************************************
			     Definizioni di tipo
*******************************************************************************/

/* 
 * Booleani e relativi valori.
 */

typedef unsigned char bool;

#ifdef TRUE
#undef TRUE
#endif
#define     TRUE     ((bool)1)

#ifdef FALSE
#undef FALSE
#endif
#define     FALSE     ((bool)0)

/*
 * Identificativi delle porte.
 */
typedef uint16_t port_t;


/*******************************************************************************
				  Strutture
*******************************************************************************/

/*
 * Canali di rete.
 */

struct chan {
	/* Connected e listening socket. */
	int c_sockfd;
	int c_listfd;

	/* Indirizzi locale e remoto. */
	struct sockaddr_in c_laddr;
	struct sockaddr_in c_raddr;
};

#endif /* MH_TYPES_H */
