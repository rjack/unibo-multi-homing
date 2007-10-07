#include "h/types.h"
#include "h/channel.h"
#include "h/util.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <stdio.h>


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

bool connect_noblock (struct chan *ch);
bool listen_noblock (struct chan *ch);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
accept_connection (struct chan *ch) {
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
	return 0;
}


int
finalize_connection (struct chan *ch) {
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


void
manage_connections (struct chan* chnl) {
	/* Attiva tutti i canali che hanno una struct sockaddr_in impostata,
	 * una ancora inizializzata e il socket non valido.
	 *
	 * I canali con entrambe le struct impostate sono considerati gia'
	 * connessi; quelli con entrambe le struct inizializzate non piu'
	 * utilizzabili; quelli con i socket validi sono gia' attivati.
	 *
	 * Se e' impostata la struct locale, viene creato un socket listening
	 * e iniziata una accept non bloccante; altrimenti viene creato un
	 * socket e iniziata una connect non bloccante. */

	int i;
	bool ok;

	for (i = 0; i < CHANNELS; i++) if (channel_is_activable (&chnl[i])) {

		/*
		 * Canale da connettere.
		 */
		if (!addr_is_set (&chnl[i].c_laddr)
		    && addr_is_set (&chnl[i].c_raddr)
		    && chnl[i].c_sockfd < 0) {

			assert (chnl[i].c_listfd < 0);

			ok = connect_noblock (&chnl[i]);
			assert (ok); /* FIXME controllo errore decente. */

			/* Connect gia' conclusa, recupera nome del socket. */
			if (errno != EINPROGRESS) {
				tcp_sockname (chnl[i].c_sockfd,
				              &chnl[i].c_laddr);
		       	}
			printf ("Canale %s %s.\n", channel_name (&chnl[i]),
			        addr_is_set (&chnl[i].c_laddr) ?
			        "connesso" : "in connessione");

			channel_set_activation_condition (&chnl[i], NULL, NULL);
		}

		/*
		 * Canale da mettere in ascolto.
		 */
		else if (addr_is_set (&chnl[i].c_laddr)
		    && !addr_is_set (&chnl[i].c_raddr)
		    && chnl[i].c_listfd < 0) {

			assert (chnl[i].c_sockfd < 0);

			ok = listen_noblock (&chnl[i]);
			assert (ok); /* FIXME controllo errore decente. */

			printf ("Canale %s in ascolto.\n",
			        channel_name (&chnl[i]));

			channel_set_activation_condition (&chnl[i], NULL, NULL);
		}

		else {
			/* Se arriva qui c'e' un problema: un canale
			 * attivabile o deve essere connesso o messo in
			 * ascolto, una delle due. */
			assert (FALSE);
		}
	}
}


fd_t
set_file_descriptors (struct chan *chnl, fd_set *rdset, fd_set *wrset) {
	int i;
	fd_t max;

	assert (chnl != NULL);
	assert (rdset != NULL);
	assert (wrset != NULL);

	max = -1;
	for (i = 0; i < CHANNELS; i++) {
		/* Connessioni. */
		if (channel_is_connecting (&chnl[i])) {
			FD_SET (chnl[i].c_sockfd, wrset);
			max = MAX (chnl[i].c_sockfd, max);
		} else if (channel_is_listening (&chnl[i])) {
			FD_SET (chnl[i].c_listfd, rdset);
			max = MAX (chnl[i].c_listfd, max);
		}

		/* TODO 
		 * traffico con ritardatore e host. */
	}

	return max;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

bool
connect_noblock (struct chan *ch) {
	/* Connessione non bloccante. Crea un socket, lo imposta non bloccante
	 * ed esegue una connect (senza bind) usando la struct sockaddr_in del
	 * canale.
	 *
	 * XXX del canale ch viene modificato esclusivamente il valore di
	 * c_sockfd, e solo quando la funzione riesce. Altre modifiche sono a
	 * carico del chiamante.
	 *
	 * Ritorna FALSE se fallisce, TRUE se riesce subito o e' in corso. In
	 * quest'ultimo caso, errno = EINPROGRESS. */

	int err;

	assert (ch != NULL);
	assert (!addr_is_set (&ch->c_laddr));
	assert (addr_is_set (&ch->c_raddr));
	assert (ch->c_listfd < 0);
	assert (ch->c_sockfd < 0);

	/* Nuovo socket tcp. */
	ch->c_sockfd = xtcp_socket();

	/* Non bloccante, se fallisce RITORNA subito. */
	if (!tcp_set_block (ch->c_sockfd, FALSE)) {
		fprintf (stderr,
			 "Canale %s, impossibile impostarlo non bloccante: "
			 "%s\n", channel_name (ch), strerror (errno));
		tcp_close (&ch->c_sockfd);
		return FALSE;
	}

	/* FIXME dove disabilitare Nagle? */
	/* TODO se necessario modificare i buffer TCP qui. */

	/* Tentativo di connessione. */
	do {
		err = connect (ch->c_sockfd,
	                      (struct sockaddr *)&ch->c_raddr,
	                      sizeof (ch->c_raddr));
	} while (err == -1 && errno == EINTR);

	/* Se riuscita subito o in corso, ritorna TRUE; altrimenti FALSE. */
	if (!err || errno == EINPROGRESS) {
		if (!err) {
			errno = 0;
		}
		return TRUE;
	}

	fprintf (stderr, "Canale %s, errore di connessione: %s\n",
	         channel_name (ch), strerror (errno));
	tcp_close (&ch->c_sockfd);
	return FALSE;
}


bool
listen_noblock (struct chan *ch) {
	int err;
	bool ok;
	char *errmsg;

	assert (ch != NULL);
	assert (addr_is_set (&ch->c_laddr));
	assert (!addr_is_set (&ch->c_raddr));
	assert (ch->c_listfd < 0);
	assert (ch->c_sockfd < 0);

	ch->c_listfd = xtcp_socket ();

	err = bind (ch->c_listfd,
	            (struct sockaddr *)&ch->c_laddr,
	            sizeof (ch->c_laddr));
	if (err) {
		errmsg = "bind fallita";
		goto error;
	}

	ok = tcp_set_reusable (ch->c_listfd, TRUE);
	if (!ok) {
		errmsg = "tcp_set_reusable fallita";
		goto error;
	}

	err = listen (ch->c_listfd, 0);
	if (err) {
		errmsg = "listen fallita";
		goto error;
	}

	ok = tcp_set_block (ch->c_listfd, FALSE);
	if (!ok) {
		errmsg = "tcp_set_block fallita";
		goto error;
	}

	return TRUE;

error:
	fprintf (stderr, "Canale %s, %s: %s\n",
	         channel_name (ch), errmsg, strerror (errno));
	tcp_close (&ch->c_listfd);
	return FALSE;
}
