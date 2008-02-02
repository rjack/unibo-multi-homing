#include "h/channel.h"
#include "h/cqueue.h"
#include "h/rqueue.h"
#include "h/segment.h"
#include "h/timeout.h"
#include "h/types.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


/*******************************************************************************
			       Variabili locali
*******************************************************************************/

/* Canali. */
static struct chan ch[CHANNELS];

/* Buffer applicazione. */
static cqueue_t *host_rcvbuf;
static cqueue_t *host_sndbuf;

static rqueue_t *net_rcvbuf[NETCHANNELS];
static rqueue_t *net_sndbuf[NETCHANNELS];

/* Ultimo seqnum inviato. */
static seq_t outseq;

/* TODO
 * - contatori:
 *   - ack/nack
 *   - etc
 * - coda segmenti da ritrasmettere
 * - OPPURE coda segmenti da ricostruire. */


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int connect_noblock (cd_t cd);
static int listen_noblock (cd_t cd);
static void idle_handler (void *args);
static void mov_host2net (cd_t ncd, len_t pldlen);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

bool
channel_can_read (cd_t cd)
{
	assert (VALID_CD (cd));

	if (cd == HOSTCD) {
		return cqueue_can_read (host_rcvbuf);
	}
	return rqueue_can_read (net_rcvbuf[cd]);
}


bool
channel_can_write (cd_t cd)
{
	assert (VALID_CD (cd));

	if (cd == HOSTCD) {
		return cqueue_can_write (host_sndbuf);
	}
	return rqueue_can_write (net_sndbuf[cd]);
}


fd_t
channel_get_listfd (cd_t cd)
{
	assert (VALID_CD (cd));
	return ch[cd].c_listfd;
}


fd_t
channel_get_sockfd (cd_t cd)
{
	assert (VALID_CD (cd));
	return ch[cd].c_sockfd;
}


int
channel_init (cd_t cd, port_t listport, char *connip, port_t connport)
{
	int err;

	assert (VALID_CD (cd));

	ch[cd].c_sockfd = -1;
	ch[cd].c_listfd = -1;

	memset (&ch[cd].c_laddr, 0, sizeof (ch[cd].c_laddr));
	memset (&ch[cd].c_raddr, 0, sizeof (ch[cd].c_raddr));
	if (listport != 0) {
		assert (connip == NULL);
		assert (connport == 0);
		err = set_addr (&ch[cd].c_laddr, NULL, listport);
	} else {
		assert (connip != NULL);
		assert (connport != 0);
		err = set_addr (&ch[cd].c_raddr, connip, connport);
	}
	if (err)
		goto error;

	ch[cd].c_tcp_rcvbuf_len = 0;
	ch[cd].c_tcp_sndbuf_len = 0;

	ch[cd].c_activity = NULL;
	if (cd != HOSTCD) {
		ch[cd].c_tcp_sndbuf_len = TCP_MIN_SNDBUF_SIZE;
		ch[cd].c_activity = timeout_create (ACTIVITY_TIMEOUT,
				idle_handler, NULL, FALSE);
	}
	return 0;

error:
	return -1;
}


void
channel_invalidate (cd_t cd)
{
	/* TODO deallocare tutte le strutture dati associate al canale. */
	/* TODO impostare il canale in modo che activate_channels non lo
	 * TODO riattivi. */
}


bool
channel_is_activable (cd_t cd)
{
	assert (VALID_CD (cd));

	if (channel_is_connected (cd)
	    || channel_is_listening (cd)
	    || (!addr_is_set (&ch[cd].c_laddr)
	        && !addr_is_set (&ch[cd].c_raddr))) {
		return FALSE;
	}

	if (cd == HOSTCD) {
		/* Proxy Receiver. */
		if (channel_must_connect (cd)) {
			bool ok = FALSE;
			cd_t ncd = NETCD;
			while (!ok && ncd < NETCHANNELS) {
				ok = channel_is_connected (ncd);
				ncd++;
			}
			return ok;
		}
		/* Proxy Sender */
		return TRUE;
	}

	/* NETCD */

	/* Proxy Sender. */
	if (channel_must_connect (cd)) {
		return channel_is_connected (HOSTCD);
	}
	/* Proxy Receiver. */
	return TRUE;
}


bool
channel_is_connected (cd_t cd)
{
	assert (VALID_CD (cd));

	if (ch[cd].c_listfd < 0
	    && ch[cd].c_sockfd >= 0
	    && addr_is_set (&ch[cd].c_laddr)
	    && addr_is_set (&ch[cd].c_raddr)) {
		return TRUE;
	}
	return FALSE;
}


bool
channel_is_connecting (cd_t cd)
{
	/* Se il socket del canale e' valido ed e' impostato solo l'addr
	 * remoto e non quello locale, allora c'e' una connect non bloccante
	 * in corso. */

	assert (VALID_CD (cd));

	if (ch[cd].c_sockfd >= 0
	    && !addr_is_set (&ch[cd].c_laddr)
	    && addr_is_set (&ch[cd].c_raddr)) {
		assert (ch[cd].c_listfd < 0);
		return TRUE;
	}
	return FALSE;
}


bool
channel_is_listening (cd_t cd)
{
	/* Se il socket listening del canale e' valido ed e' impostato l'addr
	 * locale ma non quello remoto, allora il canale e' in ascolto. */

	assert (VALID_CD (cd));

	if (ch[cd].c_listfd >= 0
	    && addr_is_set (&ch[cd].c_laddr)
	    && !addr_is_set (&ch[cd].c_raddr)) {
		assert (ch[cd].c_sockfd < 0);
		return TRUE;
	}
	return FALSE;
}


bool
channel_must_connect (cd_t cd)
{
	/* Il canale deve essere connesso se l'indirizzo remoto e' impostato,
	 * ma il socket non e' un fd valido. */

	assert (VALID_CD (cd));

	if (!addr_is_set (&ch[cd].c_laddr)
	    && addr_is_set (&ch[cd].c_raddr)
	    && ch[cd].c_sockfd < 0) {
		assert (ch[cd].c_listfd < 0);
		return TRUE;
	}
	return FALSE;
}


bool
channel_must_listen (cd_t cd)
{
	/* Il canale deve essere messo in ascolto se l'indirizzo locale e'
	 * impostato, ma il socket listening non e' un fd valido. */

	assert (VALID_CD (cd));

	if (addr_is_set (&ch[cd].c_laddr)
	    && !addr_is_set (&ch[cd].c_raddr)
	    && ch[cd].c_listfd < 0) {
		assert (ch[cd].c_sockfd < 0);
		return TRUE;
	}
	return FALSE;
}


char *
channel_name (cd_t cd)
{
	/* Ritorna una stringa allocata staticamente e terminata da '\0' della
	 * forma "xxx.xxx.xxx.xxx:yyyyy - xxx.xxx.xxx.xxx:yyyyy", dove il
	 * primo e' l'indirizzo locale, il secondo quello remoto. */

	char *bufptr;
	static char bufname[46];

	assert (VALID_CD (cd));

	memset (bufname, 0, sizeof (bufname));

	/* Indirizzo locale. */
	bufptr = addrstr (&ch[cd].c_laddr, bufname);

	/* Separatore. */
	strcpy (bufptr, " - ");

	bufptr = strchr (bufptr, '\0');
	assert (bufptr != NULL);

	/* Indirizzo remoto. */
	bufptr = addrstr (&ch[cd].c_raddr, bufptr);

	/* Overflow? */
	assert (bufname[45] == '\0');

	return bufname;
}


void
channel_prepare_io (cd_t cd)
{
	assert (VALID_CD (cd));

	if (cd == HOSTCD) {
		size_t buflen;

		assert (host_rcvbuf == NULL);
		assert (host_sndbuf == NULL);

		buflen = tcp_get_buffer_size (ch[cd].c_sockfd, SO_RCVBUF);
		host_rcvbuf = cqueue_create (buflen);

		buflen = tcp_get_buffer_size (ch[cd].c_sockfd, SO_SNDBUF);
		host_sndbuf = cqueue_create (buflen);
	}
	/* NET */
	else {
		size_t buflen;

		assert (net_rcvbuf[cd] == NULL);
		assert (net_sndbuf[cd] == NULL);

		buflen = MAX (SEGMAXLEN, tcp_get_buffer_size (ch[cd].c_sockfd,
					SO_RCVBUF));
		net_rcvbuf[cd] = rqueue_create (buflen);

		buflen = tcp_get_buffer_size (ch[cd].c_sockfd, SO_SNDBUF);
		net_sndbuf[cd] = rqueue_create (buflen);

		timeout_reset (ch[cd].c_activity);
		add_timeout (ch[cd].c_activity, ACTIVITY);
	}
}

int
channel_read (cd_t cd)
{
	assert (VALID_CD (cd));
	assert (channel_is_connected (cd));
	assert (channel_can_read (cd));

	if (cd == HOSTCD) {
		return cqueue_read (ch[cd].c_sockfd, host_rcvbuf);
	}
	return rqueue_read (ch[cd].c_sockfd, net_rcvbuf[cd]);
}


int
channel_write (cd_t cd)
{
	assert (VALID_CD (cd));
	assert (channel_is_connected (cd));
	assert (channel_can_write (cd));

	if (cd == HOSTCD) {
		return cqueue_write (ch[cd].c_sockfd, host_sndbuf);
	}
	return rqueue_write (ch[cd].c_sockfd, net_sndbuf[cd]);
}


void
feed_upload (void)
{
	/* Il routing e' banale: un segmento a ogni canale, finche' ci stanno
	 * nei net o si svuota l'host.
	 * XXX round robin: id variabile statica che tiene traccia dell'ultimo
	 * XXX canale servito.
	 * XXX solo i canali connessi.
	 * XXX le code dei net devono essere sempre piu' o meno uguali. */

	/* Ogni canale inattivo o pieno spegne il proprio bit. */
	int needmask;
	/* Contatore, statico per politica round robin. */
	static cd_t ncd = NETCD;
	size_t used;
	len_t pldlen;

	needmask = 0x7;
	used = cqueue_get_used (host_rcvbuf),
	pldlen = (used < PLDDEFLEN ? used : PLDDEFLEN);
	while (needmask != 0x0 && used > 0) {
		if (channel_is_connected (ncd)
		    && rqueue_get_aval (net_sndbuf[ncd]) >= (HDRMAXLEN
		                                             + pldlen)) {
			mov_host2net (ncd, pldlen);
			used = cqueue_get_used (host_rcvbuf),
			pldlen = (used >= PLDDEFLEN ? PLDDEFLEN : used);
		} else {
			/* Spegnimento bit. */
			needmask &= ~(0x1 << ncd);
		}
		ncd = (ncd + 1) % NETCHANNELS;
	}
}


int
proxy_init (port_t hostlistport,
		char *netconnaddr[NETCHANNELS],
		port_t netconnport[NETCHANNELS],
		port_t netlistport[NETCHANNELS],
		char *hostconnaddr, port_t hostconnport)
{
	int err;
	cd_t cd;

	err = channel_init (HOSTCD, hostlistport, hostconnaddr, hostconnport);
	if (err)
		goto error;
	host_rcvbuf = NULL;
	host_sndbuf = NULL;

	for (cd = NETCD; cd < NETCHANNELS; cd++) {
		port_t listport = (netlistport ? netlistport[cd] : 0);
		port_t connport = (netconnport ? netconnport[cd] : 0);
		char *connaddr = (netconnaddr ? netconnaddr[cd] : NULL);

		err = channel_init (cd, listport, connaddr, connport);
		if (err)
			goto error;
		net_rcvbuf[cd] = NULL;
		net_sndbuf[cd] = NULL;
	}

	outseq = 0;

#if !HAVE_MSG_NOSIGNAL
	{
		int err;
		struct sigaction act;

		act.sa_handler = SIG_IGN;
		sigemptyset (&act.sa_mask);
		act.sa_flags = 0;
		err = sigaction (SIGPIPE, &act, NULL);
		if (err) {
			perror ("sigaction");
			goto error;
		}
		fprintf (stderr, "Manca MSG_NOSIGNAL, tutti i SIGPIPE
				ignorati.\n");
	}
#endif
	return 0;

error:
	return -1;
}


int
accept_connection (cd_t cd)
{
	int err;
	socklen_t raddr_len;

	assert (VALID_CD (cd));
	assert (ch[cd].c_sockfd < 0);
	assert (ch[cd].c_listfd >= 0);
	assert (addr_is_set (&ch[cd].c_laddr));
	assert (!addr_is_set (&ch[cd].c_raddr));

	do {
		raddr_len = sizeof (ch[cd].c_raddr);
		ch[cd].c_sockfd = accept (ch[cd].c_listfd,
				(struct sockaddr *)&ch[cd].c_raddr,
				&raddr_len);
	} while (ch[cd].c_sockfd < 0 && errno == EINTR);

	assert (!(ch[cd].c_sockfd < 0
	          && (errno == EAGAIN || errno == EWOULDBLOCK)));

	if (ch[cd].c_sockfd < 0) {
		fprintf (stderr, "Canale %s, accept fallita: %s\n",
				channel_name (cd), strerror (errno));
	}

	/* A prescindere dall'esito dell'accept, chiusura del socket
	 * listening. */
	tcp_close (&ch[cd].c_listfd);

	if (ch[cd].c_sockfd < 0) {
		return -1;
	}

	err = tcp_set_block (ch[cd].c_sockfd, FALSE);
	assert (!err);

	return 0;
}


void
activate_channels (void)
{
	int i;
	int err;

	for (i = 0; i < CHANNELS; i++) if (channel_is_activable (i)) {

		if (channel_must_connect (i)) {
			err = connect_noblock (i);
			assert (!err); /* FIXME controllo errore decente. */

			/* Connect gia' conclusa, recupera nome del socket. */
			if (errno != EINPROGRESS) {
				tcp_sockname (ch[i].c_sockfd,
						&ch[i].c_laddr);
				channel_prepare_io (i);
			}
			printf ("Canale %s %s.\n", channel_name (i),
					addr_is_set (&ch[i].c_laddr) ?
					"connesso" : "in connessione");
		}
		else if (channel_must_listen (i)) {
			err = listen_noblock (i);
			assert (!err); /* FIXME controllo errore decente. */

			printf ("Canale %s in ascolto.\n",
					channel_name (i));
		}
		else {
			assert (FALSE);
		}
	}
}


int
finalize_connection (cd_t cd)
{
	int err;
	int optval;
	socklen_t optsize;

	assert (VALID_CD (cd));
	assert (ch[cd].c_sockfd >= 0);
	assert (ch[cd].c_listfd < 0);
	assert (!addr_is_set (&ch[cd].c_laddr));
	assert (addr_is_set (&ch[cd].c_raddr));

	optsize = sizeof (optval);

	/*
	 * Verifica esito connessione.
	 */
	err = getsockopt (ch[cd].c_sockfd, SOL_SOCKET, SO_ERROR, &optval,
			&optsize);
	if (err) {
		fprintf (stderr, "Canale %s, getsockopt in "
				"finalize_connection fallita: %s\n",
				channel_name (cd), strerror (errno));
		return -1;
	}

	if (optval != 0) {
		fprintf (stderr, "Canale %s, tentativo di connessione "
				"fallito.\n", channel_name (cd));
		return -1;
	}

	/*
	 * Connessione riuscita.
	 */
	tcp_sockname (ch[cd].c_sockfd, &ch[cd].c_laddr);

	return 0;
}


fd_t
set_file_descriptors (fd_set *rdset, fd_set *wrset)
{
	int i;
	fd_t max;

	assert (rdset != NULL);
	assert (wrset != NULL);

	max = -1;
	for (i = 0; i < CHANNELS; i++) {
		/* Dati da leggere e/o scrivere. */
		if (channel_is_connected (i)) {
			if (channel_can_read (i)) {
				FD_SET (ch[i].c_sockfd, rdset);
				max = MAX (ch[i].c_sockfd, max);
			}
			if (channel_can_write (i)) {
				FD_SET (ch[i].c_sockfd, wrset);
				max = MAX (ch[i].c_sockfd, max);
			}
		}
		/* Connessioni da completare o accettare. */
		else {
			if (channel_is_connecting (i)) {
				FD_SET (ch[i].c_sockfd, wrset);
				max = MAX (ch[i].c_sockfd, max);
			} else if (channel_is_listening (i)) {
				FD_SET (ch[i].c_listfd, rdset);
				max = MAX (ch[i].c_listfd, max);
			}
		}
	}
	return max;
}

/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static int
connect_noblock (cd_t cd)
{
	/* Connessione non bloccante. Crea un socket, lo imposta non bloccante
	 * ed esegue una connect (senza bind) usando la struct sockaddr_in del
	 * canale.
	 *
	 * Ritorna -1 se fallisce, 0 se riesce subito o e' in corso. In
	 * quest'ultimo caso, errno = EINPROGRESS. */

	int err;
	struct chan *chptr;

	assert (VALID_CD (cd));
	assert (!addr_is_set (&ch[cd].c_laddr));
	assert (addr_is_set (&ch[cd].c_raddr));
	assert (ch[cd].c_listfd < 0);
	assert (ch[cd].c_sockfd < 0);

	chptr = &ch[cd];
	chptr->c_sockfd = xtcp_socket ();

	err = tcp_set_block (chptr->c_sockfd, FALSE);
	if (err)
		goto error;

	/*
	 * FIXME dove disabilitare Nagle?
	 */

	if (chptr->c_tcp_rcvbuf_len > 0) {
		err = tcp_set_buffer_size (chptr->c_sockfd, SO_RCVBUF,
				chptr->c_tcp_rcvbuf_len);
		if (err)
			goto error;
	}
	if (chptr->c_tcp_sndbuf_len > 0) {
		err = tcp_set_buffer_size (chptr->c_sockfd, SO_SNDBUF,
				chptr->c_tcp_sndbuf_len);
		if (err)
			goto error;
	}

	/* Tentativo di connessione. */
	do {
		err = connect (chptr->c_sockfd,
				(struct sockaddr *)&chptr->c_raddr,
				sizeof (chptr->c_raddr));
	} while (err == -1 && errno == EINTR);

	if (!err || errno == EINPROGRESS)
		return 0;

error:
	fprintf (stderr, "Canale %s, errore di connessione: %s\n",
			channel_name (cd), strerror (errno));
	tcp_close (&chptr->c_sockfd);
	return -1;
}


static int
listen_noblock (cd_t cd)
{
	int err;
	char *errmsg;
	struct chan *chptr;

	assert (VALID_CD (cd));
	assert (addr_is_set (&ch[cd].c_laddr));
	assert (!addr_is_set (&ch[cd].c_raddr));
	assert (ch[cd].c_listfd < 0);
	assert (ch[cd].c_sockfd < 0);

	chptr = &ch[cd];
	chptr->c_listfd = xtcp_socket ();

	err = tcp_set_reusable (chptr->c_listfd, TRUE);
	if (err) {
		errmsg = "tcp_set_reusable fallita";
		goto error;
	}

	err = bind (chptr->c_listfd, (struct sockaddr *)&chptr->c_laddr,
			sizeof (chptr->c_laddr));
	if (err) {
		errmsg = "bind fallita";
		goto error;
	}

	if (chptr->c_tcp_rcvbuf_len > 0) {
		err = tcp_set_buffer_size (chptr->c_listfd, SO_RCVBUF,
				chptr->c_tcp_rcvbuf_len);
		if (err) {
			errmsg = "tcp_set_buffer_size SO_RCVBUF fallita";
			goto error;
		}
	}


	if (chptr->c_tcp_sndbuf_len > 0) {
		err = tcp_set_buffer_size (chptr->c_listfd, SO_SNDBUF,
				chptr->c_tcp_sndbuf_len);
		if (err) {
			errmsg = "tcp_set_buffer_size SO_SNDBUF fallita";
			goto error;
		}
	}

	err = tcp_set_block (chptr->c_listfd, FALSE);
	if (err) {
		errmsg = "tcp_set_block fallita";
		goto error;
	}

	err = listen (chptr->c_listfd, 0);
	if (err) {
		errmsg = "listen fallita";
		goto error;
	}

	return 0;

error:
	fprintf (stderr, "Canale %s, %s: %s\n", channel_name (cd), errmsg,
			strerror (errno));
	tcp_close (&chptr->c_listfd);
	return -1;
}


static void
idle_handler (void *args)
{
	assert (FALSE);
	/* TODO idle_handler */
}


static void
mov_host2net (cd_t ncd, len_t pldlen)
{
	int err;
	struct segwrap *newsw;
	pld_t *pld = NULL;

	assert (VALID_CD (ncd));

	newsw = segwrap_get (outseq, pldlen);
	outseq++;

	pld = seg_pld (newsw->sw_seg);
	err = cqueue_remove (host_rcvbuf, pld, pldlen);
	assert (!err);

	rqueue_add (net_sndbuf[ncd], newsw);
}
