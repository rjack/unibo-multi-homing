#include "h/types.h"
#include "h/util.h"
#include "h/segment.h"

#include <config.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/ioctl.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

/*
 * Funzioni su struct sockaddr_in.
 */

bool
addr_is_set (struct sockaddr_in *addr)
{
	/* Ritorna FALSE se addr e' ancora inizializzata a zero, TRUE
	 * altrimenti.
	 *
	 * XXX Non controlla tutta la struttura, si affida al valore di
	 * sin_family. */

	assert (addr != NULL);

	if (addr->sin_family == AF_INET) {
		return TRUE;
	}
	return FALSE;
}


char *
addrstr (struct sockaddr_in *addr, char *buf)
{
	/* Copia la stringa in formato xxx.xxx.xxx.xxx:yyyyy nel buffer buf,
	 * che deve essere grande a sufficienza. */

	char *name;

	assert (addr != NULL);
	assert (buf != NULL);

	/* Copia dell'indirizzo ip. */
	name = (char *) inet_ntop (AF_INET, &addr->sin_addr, buf,
	                           INET_ADDRSTRLEN);
	assert (name != NULL);

	/* Copia del numero di porta. */
	name = strchr (name, '\0');
	sprintf (name, ":%d", ntohs (addr->sin_port));
	/* Posiziona name alla fine della stringa. */
	name = strchr (name, '\0');
	/* Overflow? */
	assert (name < (buf + INET_ADDRSTRLEN + 1 + 5));

	return name;
}


int
set_addr (struct sockaddr_in *addr, char *ip, port_t port)
{
	/* Imposta addr secondo l'indirizzo ip, in formato xxx.xxx.xxx.xxx, e
	 * la porta port.
	 *
	 * Ritorna -1 se fallisce, 0 se riesce. */

	assert (addr != NULL);

	memset (addr, 0, sizeof (struct sockaddr_in));
	addr->sin_family = AF_INET;

	if (ip == NULL) {
		addr->sin_addr.s_addr = htonl (INADDR_ANY);
	} else if (inet_pton (AF_INET, ip, &addr->sin_addr) == 0) {
		/* La stringa ip non ha un formato valido. */
		return -1;
	}
	addr->sin_port = htons (port);

	return 0;
}


/*
 * Funzioni su stringhe.
 */

bool
streq (char *str1, char *str2)
{
	/* Ritorna TRUE se due stringhe sono uguali. */

	int cmp;

	assert (str1 != NULL);
	assert (str2 != NULL);

	cmp = strcmp (str1, str2);

	if (cmp == 0) return TRUE;
	return FALSE;
}


/*
 * Funzioni per la gestione della memoria.
 */

void
xfree (void *ptr)
{
	/* Free sicura. */

	if (ptr != NULL) {
		free (ptr);
	}
}


void *
xmalloc (size_t size)
{
	/* Malloc sicura. */
	void *ptr;

	assert (size > 0);

	ptr = malloc (size);
	if (ptr == NULL) {
		segwrap_flush_cache ();
		ptr = malloc (size);
		if (ptr == NULL) {
			perror ("Impossibile allocare memoria");
			exit (EXIT_FAILURE);
		}
	}
	return ptr;
}


/*
 * Funzioni su socket e operazioni di rete.
 */

int
tcp_close (fd_t *fd)
{
	/* Chiude il file descriptor puntato da fd e lo inizializza a -1. */

	int err;

	assert (fd != NULL);
	assert (*fd >= 0);

	do {
		err = close (*fd);
	} while (err == -1 && errno == EINTR);

	*fd = -1;
	return err;
}


ssize_t
tcp_get_buffer_size (fd_t sockfd, int bufname)
{
	int err;
	int optval;
	socklen_t optlen;

	assert (sockfd >= 0);
	assert (bufname == SO_SNDBUF || bufname == SO_RCVBUF);

	optlen = sizeof (optval);
	err = getsockopt (sockfd, SOL_SOCKET, bufname, &optval, &optlen);
	if (err < 0) {
		return -1;
	}
#ifdef LINUX_OS
	optval = optval / 2;
#endif
	return optval;
}


int
tcp_get_used_space (fd_t fd, int buf)
{
	int amount;
	int err;

	assert (fd >= 0);
	assert (buf == SO_RCVBUF || buf == SO_SNDBUF);

	err = ioctl (fd, buf == SO_SNDBUF ? TIOCOUTQ : FIONREAD, &amount);
	if (!err)
		return amount;
	return err;
}


int
tcp_set_block (fd_t fd, bool must_block)
{
	/* Se must_block = TRUE, imposta fd come bloccante, altrimenti come
	 * non bloccante.
	 *
	 * Ritorna 0 se riesce, -1 altrimenti. */

	int flags;

	assert (fd >= 0);
	assert (must_block == TRUE || must_block == FALSE);

	flags = fcntl (fd, F_GETFL, 0);
	if (flags != -1) {
		if (must_block) {
			flags &= ~O_NONBLOCK;
		} else {
			flags |= O_NONBLOCK;
		}
		if (fcntl (fd, F_SETFL, flags) != -1) {
			return 0;
		}
	}
	return -1;
}


int
tcp_set_buffer_size (fd_t sockfd, int bufname, size_t buflen)
{

	assert (sockfd > 0);
	assert (bufname == SO_SNDBUF || bufname == SO_RCVBUF);
	assert (buflen > 0);

	return setsockopt (sockfd, SOL_SOCKET,
	                   bufname, &buflen, sizeof (buflen));
}


int
tcp_set_nagle (fd_t fd, bool active)
{
	/* Se active = TRUE imposta l'algoritmo di Nagle sul file descriptor
	 * fd, altrimenti attiva l'opzione TCP_NODELAY.
	 *
	 * Ritorna TRUE se riesce, FALSE altrimenti. */

	int err;
	int optval;

	assert (fd >= 0);
	assert (active == TRUE || active == FALSE);

	optval = (active == TRUE ? 0 : 1);

	err = setsockopt (fd, IPPROTO_TCP, TCP_NODELAY,
	                  &optval, sizeof (optval));
	return err;
}


int
tcp_set_reusable (fd_t fd, bool reusable)
{
	int err;
	int optval;

	assert (fd >= 0);
	assert (reusable == TRUE || reusable == FALSE);

	optval = (reusable == TRUE ? 1 : 0);

	err = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR,
	                  &optval, sizeof (optval));

	return err;
}


void
tcp_sockname (fd_t fd, struct sockaddr_in *laddr)
{
	/* Wrapper per nascondere le bruttezze di getsockname. */

	int err;
	socklen_t len;

	assert (fd >= 0);
	assert (laddr != NULL);
	assert (!addr_is_set (laddr));

	len = sizeof (*laddr);

	err = getsockname (fd, (struct sockaddr *) laddr, &len);
	assert (!err);
}


fd_t
xtcp_socket (void)
{
	/* Socket sicura. */

	fd_t newfd = socket (AF_INET, SOCK_STREAM, 0);
	if (newfd < 0) {
		perror ("Impossibile creare il socket");
		exit (EXIT_FAILURE);
	}
	return newfd;
}
