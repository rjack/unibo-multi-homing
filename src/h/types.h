#ifndef MH_TYPES_H
#define MH_TYPES_H

#include <arpa/inet.h>
#include <inttypes.h>
#include <sys/time.h>


/*******************************************************************************
			     Definizioni di tipo
*******************************************************************************/

/*
 * Booleani e relativi valori.
 */
typedef unsigned char bool;

#ifdef FALSE
#undef FALSE
#endif
#define     FALSE     ((bool)0)

#ifdef TRUE
#undef TRUE
#endif
#define     TRUE     (!FALSE)

/*
 * Identificativi delle porte.
 */
typedef uint16_t port_t;

/*
 * Descrittori.
 */
typedef int fd_t;     /* file e socket */
typedef int cd_t;     /* canali */

/*
 * Puntatori a funzione.
 */
typedef void (*timeout_handler_t)(void *args);
typedef bool (*condition_checker_t)(void *args);
typedef int (*io_performer_t)(fd_t fd, void *args);

/*
 * Campi dei segmenti.
 */
/* Numeri di sequenza. */
typedef uint8_t seq_t;
/* Lunghezza del segmento. */
typedef uint8_t len_t;
/* Dati */
typedef uint8_t pld_t;
/* Flag conformazione segmento. */
typedef uint8_t flag_t;
/* Buffer contentente un segmento. */
typedef uint8_t seg_t;


/*******************************************************************************
				   Costanti
*******************************************************************************/

/* Numero di canali di rete. */
#define     NETCHANNELS    3
/* Numero di canali totali. */
#define     CHANNELS       (NETCHANNELS + 1)
/* Channel descriptor del primo canale di rete. */
#define     NETCD            0
/* Channel descriptor del canale dell'host. */
#define     HOSTCD           (NETCHANNELS)


/*
 * Durate dei timeout in secondi.
 */
#define     ACTIVITY_TIMEOUT     100000000.0 /* 0.250 */
#define     NAK_TIMEOUT          0.130
#define     ACK_TIMEOUT          2

/*
 * Segmenti.
 */
/* Indici dei campi. */
#define     FLG     0
#define     SEQ     1
#define     LEN     2

/* Dimensione campi, in byte. */
#define     SEQLEN     sizeof(seq_t)
#define     LENLEN     sizeof(len_t)
#define     FLGLEN     sizeof(flag_t)

/* Limiti dei segmenti, in byte. */
#define     HDRMINLEN     (FLGLEN + SEQLEN)
#define     HDRMAXLEN     (FLGLEN + SEQLEN + LENLEN)

/* XXX sono possibili payload minori di PLDMINLEN, e' solo indicativo.
 * XXX PLDMAXLEN invece e' un limite reale. */
#define     PLDMINLEN     (HDRMAXLEN * 2)
#define     PLDMAXLEN     UINT8_MAX
#define     PLDDEFLEN     (PLDMAXLEN - (PLDMINLEN - 1))  /* TODO da tarare */

#define     SEGMINLEN     (PLDMINLEN + HDRMAXLEN)
#define     SEGMAXLEN     (PLDMAXLEN + HDRMAXLEN)

/* Bit del campo flag */
#define     CRTFLAG     0x1
#define     PLDFLAG     0x2
#define     LENFLAG     0x4
#define     NAKFLAG     0x8
#define     ACKFLAG     0x10


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
	size_t c_tcp_rcvbuf_len;
	size_t c_tcp_sndbuf_len;

	/* Timeout di attivita'. */
	timeout_t *c_activity;
};


/*
 * Coda circolare a dimensione fissa.
 */
typedef struct {
	/* TRUE quando tail e' minore di head, distingue coda completamente
	 * vuota da completamente piena. */
	bool cq_wrap;

	/* Puntatore al buffer. */
	seg_t *cq_data;

	/* Dimensione del buffer. */
	size_t cq_len;

	/* Testa e coda. */
	size_t cq_head;
	size_t cq_tail;
} cqueue_t;


/*
 * Wrapper per creare code di segmenti.
 */
struct segwrap {
	seg_t *sw_seg;
	size_t sw_seglen;
	struct segwrap *sw_next;
	struct segwrap *sw_prev;
};


/*
 * Coda di routing.
 */
typedef struct {
	/* Buffer circolare per I/O sul sockfd. */
	cqueue_t *rq_data;
	/* Coda dei segmenti in uscita. */
	struct segwrap *rq_seg;
	/* Coda dei segmenti spediti. */
	struct segwrap *rq_sent;
	/* In invio e' il numero di byte da spedire per completare il segmento
	 * corrente, in ricezione il numero di byte che mancano alla fine del
	 * segmento in arrivo. */
	size_t rq_nbytes;
} rqueue_t;

#endif /* MH_TYPES_H */
