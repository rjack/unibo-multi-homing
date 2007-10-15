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
/* Id del primo canale di rete. */
#define     NET            0
/* Id del canale dell'host: nell'array viene subito dopo a quelli di rete. */
#define     HOST           (NETCHANNELS)


/*******************************************************************************
				    Macro
*******************************************************************************/

/*
 * Massimo e minimo.
 */
#ifdef MAX
#undef MAX
#endif
#define     MAX(a,b)     ((a) > (b) ? (a) : (b))

#ifdef MIN
#undef MIN
#endif
#define     MIN(a,b)     ((a) < (b) ? (a) : (b))


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

/*
 * File descriptor.
 */
typedef int fd_t;


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

	/* Funzione che decide quando il canale sia attivabile. */
	bool (*c_is_activable)(void *);

	/* Argomento alla funzione. */
	void *c_activable_arg;
};


/*
 * Proxy: i vari canali, i buffer e i contatori.
 */

struct proxy {
	/* Canale con l'host. */
	struct chan p_host;
	/* Canali con il ritardatore. */
	struct chan p_net[NETCHANNELS];
	/* Array di puntatori ai canali, per facilitare iterazioni. */
	struct chan *p_chptr[CHANNELS];

	/* TODO
	 * - buffer di trasferimento con l'host
	 * - coda accodamento dati in attesa di smistamento ai canali
	 * - buffer di trasferimento con il ritardatore
	 * - contatori:
	 *   - ultimo seqnum inviato
	 *   - ack/nack
	 *   - etc
	 * - coda segmenti da ritrasmettere
	 * - OPPURE coda segmenti da ricostruire. */
};


#endif /* MH_TYPES_H */
