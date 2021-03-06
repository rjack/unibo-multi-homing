#ifndef UTIL_H
#define UTIL_H

#include "types.h"

#include <netinet/in.h>


/*******************************************************************************
				    Macro
*******************************************************************************/

/* Massimo e minimo. */
#ifdef MAX
#undef MAX
#endif
#define     MAX(a,b)     ((a) > (b) ? (a) : (b))

#ifdef MIN
#undef MIN
#endif
#define     MIN(a,b)     ((a) < (b) ? (a) : (b))


/* Test sui valori dei bool e dei chan descriptor: usati nelle assert. */
#define     BOOL_VALUE(b)     ((b) == FALSE || (b) == TRUE)
#define     VALID_CD(cd)      ((cd) >= 0 && (cd) < CHANNELS)


/* Valore assoluto. */
#define     ABS(n)     ((n) >= 0 ? (n) : -1 * (n))

/*******************************************************************************
				  Prototipi
*******************************************************************************/

/*
 * Funzioni su struct sockaddr_in.
 */

bool
addr_is_set (struct sockaddr_in *addr);


char *
addrstr (struct sockaddr_in *addr, char *buf);


int
set_addr (struct sockaddr_in *addr, char *ip, port_t port);


/*
 * Funzioni su stringhe.
 */

bool
streq (char *str1, char *str2);


/*
 * Funzioni per la gestione della memoria.
 */

void
xfree (void *ptr);


void *
xmalloc (size_t size);


/*
 * Funzioni su socket e operazioni di rete.
 */

int
tcp_close (fd_t *fd);


ssize_t
tcp_get_buffer_size (fd_t sockfd, int bufname);


int
tcp_get_used_space (fd_t fd, int buf);


int
tcp_set_block (fd_t fd, bool must_block);


int
tcp_set_buffer_size (fd_t sockfd, int bufname, size_t buflen);


int
tcp_set_nagle (fd_t fd, bool active);


int
tcp_set_reusable (fd_t fd, bool reusable);


void
tcp_sockname (fd_t fd, struct sockaddr_in *laddr);


fd_t
xtcp_socket (void);


#endif /* UTIL_H */
