#ifndef MH_TYPES_H
#define MH_TYPES_H

#include <arpa/inet.h>
#include <sys/time.h>


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
 * Cronometri, per misurazioni temporali.
 */
typedef struct {
	double cr_elapsed;
	struct timeval cr_start;
} crono_t;


/*
 * Timeout, per controllare scadenze.
 */
typedef struct {
	double to_maxval;
	crono_t to_crono;
} timeout_t;


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

	/* Dimensioni dei buffer tcp. */
	size_t c_rcvbuf_len;
	size_t c_sndbuf_len;

	/* Funzione che decide quando il canale sia attivabile. */
	bool (*c_is_activable)(void *);

	/* Funzioni che controllano le condizioni in cui il canale puo' fare
	 * I/O sul suo c_sockfd. */
	bool (*c_can_read)(void *);
	bool (*c_can_write)(void *);

	/* Puntatori per gli argomenti delle funzioni. */
	void *c_activable_arg;
	void *c_can_read_arg;
	void *c_can_write_arg;
};


/*
 * Coda circolare a dimensione fissa.
 */
typedef struct {
	/* TRUE quando tail e' minore di head, distingue coda completamente
	 * vuota da completamente piena. */
	bool cq_wrap;

	/* Puntatore al buffer. */
	char *cq_data;

	/* Dimensione del buffer. */
	size_t cq_len;

	/* Testa e coda. */
	size_t cq_head;
	size_t cq_tail;
} cqueue_t;


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

	cqueue_t *p_host_rcvbuf;
	cqueue_t *p_host_sndbuf;
	/* TODO
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
