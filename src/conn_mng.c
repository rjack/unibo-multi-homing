#include "h/types.h"
#include "h/channel.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdio.h>


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int connect_noblock (struct chan *ch);
static int listen_noblock (struct chan *ch);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
accept_connection (struct chan *ch)
{
	int err;
	socklen_t raddr_len;

	assert (ch != NULL);
	assert (ch->c_sockfd < 0);
	assert (ch->c_listfd >= 0);
	assert (addr_is_set (&ch->c_laddr));
	assert (!addr_is_set (&ch->c_raddr));

	do {
		raddr_len = sizeof (ch->c_raddr);
		ch->c_sockfd = accept (ch->c_listfd,
		                       (struct sockaddr *)&ch->c_raddr,
		                       &raddr_len);
	} while (ch->c_sockfd < 0 && errno == EINTR);

	assert (!(ch->c_sockfd < 0
	          && (errno == EAGAIN || errno == EWOULDBLOCK)));

	if (ch->c_sockfd < 0) {
		fprintf (stderr,
		         "Canale %s, accept fallita: %s\n",
		         channel_name (ch), strerror (errno));
	}

	/* A prescindere dall'esito dell'accept, chiusura del socket
	 * listening. */
	tcp_close (&ch->c_listfd);

	if (ch->c_sockfd < 0) {
		return -1;
	}

	err = tcp_set_block (ch->c_sockfd, FALSE);
	assert (!err);

	return 0;
}


void
activate_channels (struct chan* chnl[CHANNELS])
{
	int i;
	int err;

	for (i = 0; i < CHANNELS; i++) if (channel_is_activable (chnl[i])) {

		if (channel_must_connect (chnl[i])) {
			err = connect_noblock (chnl[i]);
			assert (!err); /* FIXME controllo errore decente. */

			/* Connect gia' conclusa, recupera nome del socket. */
			if (errno != EINPROGRESS) {
				tcp_sockname (chnl[i]->c_sockfd,
				              &chnl[i]->c_laddr);
		       	}
			printf ("Canale %s %s.\n", channel_name (chnl[i]),
			        addr_is_set (&chnl[i]->c_laddr) ?
			        "connesso" : "in connessione");
		}

		else if (channel_must_listen (chnl[i])) {
			err = listen_noblock (chnl[i]);
			assert (!err); /* FIXME controllo errore decente. */

			printf ("Canale %s in ascolto.\n",
			        channel_name (chnl[i]));
		}

		else {
			assert (FALSE);
		}

		/* Comunque vada il canale non va piu' attivato. */
		channel_set_condition (chnl[i], SET_ACTIVABLE, NULL, NULL);
	}
}


int
finalize_connection (struct chan *ch)
{
	int err;
	int optval;
	socklen_t optsize;

	assert (ch != NULL);
	assert (ch->c_sockfd >= 0);
	assert (ch->c_listfd < 0);
	assert (!addr_is_set (&ch->c_laddr));
	assert (addr_is_set (&ch->c_raddr));

	optsize = sizeof (optval);

	/*
	 * Verifica esito connessione.
	 */
	err = getsockopt (ch->c_sockfd, SOL_SOCKET, SO_ERROR, &optval,
	                  &optsize);
	if (err) {
		fprintf (stderr,
		         "Canale %s, getsockopt in finalize_connection "
		         "fallita: %s\n",
		         channel_name (ch), strerror (errno));
		return -1;
       	}

	if (optval != 0) {
		fprintf (stderr,
		         "Canale %s, tentativo di connessione fallito.\n",
		         channel_name (ch));
		return -1;
	}

	/*
	 * Connessione riuscita.
	 */
	tcp_sockname (ch->c_sockfd, &ch->c_laddr);

	return 0;
}


fd_t
set_file_descriptors (struct chan *chnl[CHANNELS],
                      fd_set *rdset, fd_set *wrset)
{
	int i;
	fd_t max;

	assert (chnl != NULL);
	assert (rdset != NULL);
	assert (wrset != NULL);

	max = -1;
	for (i = 0; i < CHANNELS; i++) {
		/* Dati da leggere e/o scrivere. */
		if (channel_is_connected (chnl[i])) {
			if (channel_can_read (chnl[i])) {
				FD_SET (chnl[i]->c_sockfd, rdset);
				max = MAX (chnl[i]->c_sockfd, max);
			}
			if (channel_can_write (chnl[i])) {
				FD_SET (chnl[i]->c_sockfd, wrset);
				max = MAX (chnl[i]->c_sockfd, max);
			}
		}
		/* Connessioni da completare o accettare. */
		else {
			if (channel_is_connecting (chnl[i])) {
				FD_SET (chnl[i]->c_sockfd, wrset);
				max = MAX (chnl[i]->c_sockfd, max);
			} else if (channel_is_listening (chnl[i])) {
				FD_SET (chnl[i]->c_listfd, rdset);
				max = MAX (chnl[i]->c_listfd, max);
			}
		}
	}

	return max;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static int
connect_noblock (struct chan *ch)
{
	/* Connessione non bloccante. Crea un socket, lo imposta non bloccante
	 * ed esegue una connect (senza bind) usando la struct sockaddr_in del
	 * canale.
	 *
	 * XXX del canale ch viene modificato esclusivamente il valore di
	 * c_sockfd, e solo quando la funzione riesce. Altre modifiche sono a
	 * carico del chiamante.
	 *
	 * Ritorna -1 se fallisce, 0 se riesce subito o e' in corso. In
	 * quest'ultimo caso, errno = EINPROGRESS. */

	int err;

	assert (ch != NULL);
	assert (!addr_is_set (&ch->c_laddr));
	assert (addr_is_set (&ch->c_raddr));
	assert (ch->c_listfd < 0);
	assert (ch->c_sockfd < 0);

	ch->c_sockfd = xtcp_socket ();

	err = tcp_set_block (ch->c_sockfd, FALSE);
	if (err) {
		goto error;
	}

	/*
	 * FIXME dove disabilitare Nagle?
	 */

	if (ch->c_rcvbuf_len > 0) {
		err = tcp_set_buffer_size (ch->c_sockfd, SO_RCVBUF,
		                           ch->c_rcvbuf_len);
		assert (!err);
		assert (ch->c_rcvbuf_len == tcp_get_buffer_size (ch->c_sockfd,
		                                                 SO_RCVBUF));
	}
	if (ch->c_sndbuf_len > 0) {
		err = tcp_set_buffer_size (ch->c_sockfd, SO_SNDBUF,
		                           ch->c_sndbuf_len);
		assert (!err);
		assert (ch->c_sndbuf_len == tcp_get_buffer_size (ch->c_sockfd,
		                                                 SO_SNDBUF));
	}

	/* Tentativo di connessione. */
	do {
		err = connect (ch->c_sockfd,
	                      (struct sockaddr *)&ch->c_raddr,
	                      sizeof (ch->c_raddr));
	} while (err == -1 && errno == EINTR);

	/* Se riuscita subito o in corso, fix errno e ritorna. */
	if (!err || errno == EINPROGRESS) {
		if (!err) {
			errno = 0;
		}
		return 0;
	}

error:
	fprintf (stderr, "Canale %s, errore di connessione: %s\n",
	         channel_name (ch), strerror (errno));
	tcp_close (&ch->c_sockfd);
	return -1;
}


static int
listen_noblock (struct chan *ch)
{
	int err;
	char *errmsg;

	assert (ch != NULL);
	assert (addr_is_set (&ch->c_laddr));
	assert (!addr_is_set (&ch->c_raddr));
	assert (ch->c_listfd < 0);
	assert (ch->c_sockfd < 0);

	ch->c_listfd = xtcp_socket ();

	err = tcp_set_reusable (ch->c_listfd, TRUE);
	if (err) {
		errmsg = "tcp_set_reusable fallita";
		goto error;
	}

	err = bind (ch->c_listfd,
	            (struct sockaddr *)&ch->c_laddr,
	            sizeof (ch->c_laddr));
	if (err) {
		errmsg = "bind fallita";
		goto error;
	}

	if (ch->c_rcvbuf_len > 0) {
		err = tcp_set_buffer_size (ch->c_listfd, SO_RCVBUF,
		                           ch->c_rcvbuf_len);
		if (err) {
			errmsg = "tcp_set_buffer_size SO_RCVBUF fallita";
			goto error;
		}
		assert (ch->c_rcvbuf_len == tcp_get_buffer_size (ch->c_listfd,
		                                                 SO_RCVBUF));
	}


	if (ch->c_sndbuf_len > 0) {
		err = tcp_set_buffer_size (ch->c_listfd, SO_SNDBUF,
		                           ch->c_sndbuf_len);
		if (err) {
			errmsg = "tcp_set_buffer_size SO_SNDBUF fallita";
			goto error;
		}
		assert (ch->c_sndbuf_len == tcp_get_buffer_size (ch->c_listfd,
		                                                 SO_SNDBUF));
	}

	err = tcp_set_block (ch->c_listfd, FALSE);
	if (err) {
		errmsg = "tcp_set_block fallita";
		goto error;
	}

	err = listen (ch->c_listfd, 0);
	if (err) {
		errmsg = "listen fallita";
		goto error;
	}

	return 0;

error:
	fprintf (stderr, "Canale %s, %s: %s\n",
	         channel_name (ch), errmsg, strerror (errno));
	tcp_close (&ch->c_listfd);
	return -1;
}
