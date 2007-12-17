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

/*
 * Durate dei timeout.
 */
#define     ACTIVITY_TIMEOUT     0.250
#define     NAK_TIMEOUT          0.130
#define     ACK_TIMEOUT          2


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

/*
 * Puntatori a funzione.
 */
typedef void (*timeout_handler_t)(void *args);
typedef bool (*condition_checker_t)(void *args);
typedef int (*io_performer_t)(fd_t fd, void *args);


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
 * Timeout, eseguono un trigger allo scadere di un intervallo di tempo.
 */
typedef struct timeout_t {
	/* Se TRUE la struct deve essere deallocata dopo l'esecuzione del
	 * trigger. */
	bool to_oneshot;

	/* Durata del timeout e cronometro. */
	double to_maxval;
	crono_t to_crono;

	/* Funzione da eseguire allo scadere. */
	timeout_handler_t to_trigger;
	void *to_trigger_args;

	/* Per la coda mantenuta dal timeout_manager. */
	struct timeout_t *to_next;
	struct timeout_t *to_prev;
} timeout_t;

/*
 * Classi di timeout.
 */

typedef enum {
	ACTIVITY, NACK, ACK
} timeout_class;


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

	/* Puntatori ai buffer applicazione. */
	void *c_rcvbufptr;
	void *c_sndbufptr;

	/* Timeout di attivita'. */
	timeout_t *c_activity;

	/* Funzione che decide quando il canale sia attivabile. */
	condition_checker_t c_is_activable;

	/* Funzioni che controllano le condizioni in cui il canale puo' fare
	 * I/O sul suo c_sockfd. */
	condition_checker_t c_can_read;
	condition_checker_t c_can_write;

	/* Funzioni di I/O sul sockfd. */
	io_performer_t c_read;
	io_performer_t c_write;

	/* Puntatori per gli argomenti delle funzioni. */
	void *c_activable_args;
	void *c_can_read_args;
	void *c_can_write_args;
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


/*
 * Argomenti per i timeout_handler_t.
 */

/* Per idle_handler. */
struct idle_args {
	struct proxy *ia_px;
	struct chan *ia_ch;
};

/* Per ack_handler. */
struct ack_args {
	struct proxy *aa_px;
};

/* TODO struct nak_args */



#endif /* MH_TYPES_H */
