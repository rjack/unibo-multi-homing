#include "h/channel.h"
#include "h/cqueue.h"
#include "h/rqueue.h"
#include "h/segment.h"
#include "h/seghash.h"
#include "h/timeout.h"
#include "h/types.h"
#include "h/util.h"

#include <config.h>
#include <string.h>
#if !HAVE_MSG_NOSIGNAL
#include <signal.h>
#endif

#define     TYPE     struct segwrap
#define     NEXT     sw_next
#define     PREV     sw_prev
#define     EMPTYQ   NULL
#include "src/queue_template"


/*******************************************************************************
			       Variabili locali
*******************************************************************************/

/* Canali. */
static struct chan ch[CHANNELS];

/* Contatore statico per politica round robin. */
static cd_t rrcd;

/* Buffer applicazione. */
static cqueue_t *host_rcvbuf;
static cqueue_t *host_sndbuf;
static rqueue_t *net_rcvbuf[NETCHANNELS];
static rqueue_t *net_sndbuf[NETCHANNELS];

/* Code dei segmenti urgenti. */
static struct segwrap *urgentq;
/* Code urgenti per i segmenti assegnati a un particolare canale con il con il
 * ritardatore; contiente CRIT, ACK e NAK. */
static struct segwrap *urgentq_priv[NETCHANNELS];

/* Coda dei segmenti ricevuti dal ritardatore. */
static struct segwrap *joinq;

/* Ultimo seqnum inviato all ritardatore. */
static seq_t outseq;
/* Ultimo seqnum inviato all'host. */
static seq_t last_sent;

/* Ultimo ack ricevuto dal Proxy Sender. */
static struct segwrap *last_ack_rcvd;
static bool ack_handled;


/*******************************************************************************
				    Macro
*******************************************************************************/

#define     ROTATE_RRCD     (rrcd = (rrcd + 1) % NETCHANNELS)


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int connect_noblock (cd_t cd);
static int listen_noblock (cd_t cd);
static void host2net (void);
static void urg2net (void);
static void netsndbuf_rm_acked (struct segwrap *ack);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
channel_activity_notice (cd_t cd)
{
	assert (VALID_CD (cd));
	timeout_reset (ch[cd].c_activity);
}


bool
channel_can_read (cd_t cd)
{
	assert (VALID_CD (cd));

	if (cd == HOSTCD)
		return cqueue_can_read (host_rcvbuf);
	return rqueue_can_read (net_rcvbuf[cd]);
}


bool
channel_can_write (cd_t cd)
{
	assert (VALID_CD (cd));

	if (cd == HOSTCD)
		return cqueue_can_write (host_sndbuf);
	return rqueue_can_write (net_sndbuf[cd]);
}


void
channel_close (cd_t cd)
{
	fprintf (stderr, "Canale %d CHIUSO\n", cd);

	/* Chiusura socket. */
	if (ch[cd].c_listfd >= 0)
		tcp_close (&ch[cd].c_listfd);
	if (ch[cd].c_sockfd >= 0)
		tcp_close (&ch[cd].c_sockfd);

	/* Svuotamento segmenti. */
	if (cd != HOSTCD) {
		while (!isEmpty (net_sndbuf[cd]->rq_sgmt)) {
			struct segwrap *sw;
			sw = qdequeue (&net_sndbuf[cd]->rq_sgmt);
			urgent_add (sw);
		}
		while (!isEmpty (urgentq_priv[cd]))
			segwrap_destroy (qdequeue (&urgentq_priv[cd]));
	}

	channel_invalidate (cd);
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
		ch[cd].c_activity = timeout_create (TOACT_VAL, channel_close,
				cd, FALSE);
	}

	return 0;

error:
	return -1;
}


void
channel_invalidate (cd_t cd)
{
	/* Dealloca tutte le strutture dati associate al canale. */

	if (cd == HOSTCD) {
		cqueue_destroy (host_rcvbuf);
		cqueue_destroy (host_sndbuf);
	} else {
		rqueue_destroy (net_rcvbuf[cd]);
		rqueue_destroy (net_sndbuf[cd]);
		net_rcvbuf[cd] = NULL;
		net_sndbuf[cd] = NULL;
	}

	/* Timeout attivita'. */
	if (ch[cd].c_activity != NULL) {
		del_timeout (ch[cd].c_activity, TOACT);
		timeout_destroy (ch[cd].c_activity);
	}

	/* Reinizializzazione campi. */
	memset (&ch[cd].c_laddr, 0, sizeof(ch[cd].c_laddr));
	memset (&ch[cd].c_raddr, 0, sizeof(ch[cd].c_raddr));

	ch[cd].c_tcp_rcvbuf_len = -1;
	ch[cd].c_tcp_sndbuf_len = -1;

	ch[cd].c_activity = NULL;
}


bool
channel_is_activable (cd_t cd)
{
	assert (VALID_CD (cd));

	if (channel_is_connected (cd)
	    || channel_is_listening (cd)
	    || (!addr_is_set (&ch[cd].c_laddr)
	        && !addr_is_set (&ch[cd].c_raddr)))
		return FALSE;

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
	if (channel_must_connect (cd))
		return channel_is_connected (HOSTCD);
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
	    && addr_is_set (&ch[cd].c_raddr))
		return TRUE;

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
	int err;

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

		buflen = MAX (SEGMAXLEN,
				tcp_get_buffer_size (ch[cd].c_sockfd,
					SO_RCVBUF))
			+ SEGMAXLEN;
		net_rcvbuf[cd] = rqueue_create (buflen);

		buflen = SEGMAXLEN;
		net_sndbuf[cd] = rqueue_create (buflen);

		timeout_reset (ch[cd].c_activity);
		add_timeout (ch[cd].c_activity, TOACT);
	}

	/* Disabilita Nagle. */
	err = tcp_set_nagle (ch[cd].c_sockfd, FALSE);
	if (err) {
		perror ("tcp_set_nagle");
		fprintf (stderr, "!!! Canale %d TCP_NODELAY fallito!\n", cd);
	}
}


int
channel_read (cd_t cd)
{
	assert (VALID_CD (cd));
	assert (channel_is_connected (cd));
	assert (channel_can_read (cd));

	if (cd == HOSTCD)
		return cqueue_read (ch[cd].c_sockfd, host_rcvbuf);
	return  rqueue_read (ch[cd].c_sockfd, net_rcvbuf[cd]);
}


int
channel_write (cd_t cd)
{
	assert (VALID_CD (cd));
	assert (channel_is_connected (cd));
	assert (channel_can_write (cd));

	if (cd == HOSTCD)
		return cqueue_write (ch[cd].c_sockfd, host_sndbuf);
	return rqueue_write (ch[cd].c_sockfd, net_sndbuf[cd]);
}


void
feed_download (void)
{
	int err;
	struct segwrap *head;

	while ((head = getHead (joinq)) != NULL
	       && seqcmp (seg_seq (head->sw_seg), last_sent + 1) == 0
	       && seg_pld_len (head->sw_seg) <= cqueue_get_aval (host_sndbuf))
	{
		head = qdequeue (&joinq);
		err = cqueue_add (host_sndbuf,
				seg_pld (head->sw_seg),
				seg_pld_len (head->sw_seg));
		assert (!err);
		last_sent = seg_seq (head->sw_seg);
#ifdef VERBOSE
		printf ("last sent = %d\n", last_sent);
		segwrap_print ("feed_download", head);
#endif
		segwrap_destroy (head);
	}
}


void
feed_upload (void)
{
	assert (last_ack_rcvd != NULL);
	if (!ack_handled) {
		assert (seg_is_ack (last_ack_rcvd->sw_seg));
		netsndbuf_rm_acked (last_ack_rcvd);
		ack_handled = TRUE;
	}
	urg2net ();
	if (urgent_empty ())
		host2net ();
}


cd_t
get_cd_from (void *element, int type)
{
	cd_t cd;
	cd_t retcd;

	assert (element != NULL);

	retcd = -1;
	switch (type) {
	case ELRQUEUE :
		for (cd = NETCD; cd < NETCD + NETCHANNELS; cd++)
			if (net_rcvbuf[cd] == (rqueue_t *)element
			    || net_sndbuf[cd] == (rqueue_t *)element)
				retcd = cd;
		break;
	default:
		assert (FALSE);
	}
	return retcd;
}


seq_t
get_last_sent_to_host (void)
{
	return last_sent;
}


void
join_add (struct segwrap *sw)
{
	seq_t s;
	seq_t seqsw;
	struct segwrap *head;

	assert (sw != NULL);

	seqsw = seg_seq (sw->sw_seg);

	/* Segmento vecchio, scartato. */
	if (seqcmp (seqsw, last_sent) <= 0) {
#ifdef VERBOSE
		printf ("last sent = %d\n", last_sent);
		segwrap_print ("join_add scarta vecchio", sw);
#endif
		segwrap_destroy (sw);
	}
	/* Coda joinq vuota. */
	else if ((head = getHead (joinq)) == NULL) {
		if (seqcmp (seqsw, last_sent + 1) != 0)
			for (s = last_sent + 1; seqcmp (s, seqsw) < 0; s++)
				add_nak_timeout (s);
		qenqueue (&joinq, sw);
	}
	/* Coda joinq non vuota. */
	else {
		seq_t seqhd;
		seq_t seqtl;
		seqhd = seg_seq (head->sw_seg);
		seqtl = seg_seq (joinq->sw_seg);
		/* Maggiore della coda. */
		if (seqcmp (seqsw, seqtl + 1) > 0) {
			for (s = seqtl + 1; seqcmp (s, seqsw) < 0; s++)
				add_nak_timeout (s);
			qenqueue (&joinq, sw);
		}
		/* Successivo a quello in coda. */
		else if (seqcmp (seqsw, seqtl + 1) == 0)
			qenqueue (&joinq, sw);
		/* Precedente alla testa. */
		else if (seqcmp (seqsw, seqhd) < 0) {
			del_nak_timeout (seqsw);
			qpush (&joinq, sw);
		}
		/* Tra testa e coda. */
		else {
			del_nak_timeout (seqsw);
			qinorder_insert (&joinq, sw, segwrap_seqcmp);
			/* Annulla inserimento se duplicato. */
			if (seg_seq (sw->sw_next->sw_seg) == seqsw
			    || seg_seq (sw->sw_prev->sw_seg) == seqsw) {
				qremove (&joinq, sw);
				segwrap_destroy (sw);
			}
		}
	}
}


bool
urgent_empty (void)
{
	cd_t cd;

	for (cd = NETCD; cd < NETCD + NETCHANNELS; cd++)
		if (channel_is_connected (cd)
		    && (urgent_head (cd) != NULL))
			return FALSE;
	return TRUE;
}


int
proxy_init (port_t hostlistport,
		char *netconnaddr[NETCHANNELS],
		port_t netconnport[NETCHANNELS],
		port_t netlistport[NETCHANNELS],
		char *hostconnaddr, port_t hostconnport)
{
	/* Inizializza le strutture dati del proxy. */

	int err;
	cd_t cd;

	/* Canale con l'host e relativi buffer applicazione. */
	err = channel_init (HOSTCD, hostlistport, hostconnaddr, hostconnport);
	if (err)
		goto error;
	host_rcvbuf = NULL;
	host_sndbuf = NULL;

	/* Canali con il ritardatore, relativi buffer applicazione e code
	 * urgenti. */
	for (cd = NETCD; cd < NETCD + NETCHANNELS; cd++) {
		port_t listport = (netlistport ? netlistport[cd] : 0);
		port_t connport = (netconnport ? netconnport[cd] : 0);
		char *connaddr = (netconnaddr ? netconnaddr[cd] : NULL);

		err = channel_init (cd, listport, connaddr, connport);
		if (err)
			goto error;
		net_rcvbuf[cd] = NULL;
		net_sndbuf[cd] = NULL;
		urgentq_priv[cd] = newQueue ();
	}

	/* Contatori numeri di sequenza. */
	outseq = 0;
	last_sent = SEQMAX;

	/* Code di segmenti. */
	joinq = newQueue ();
	urgentq = newQueue ();

	/* Indice round robin per routing. */
	rrcd = NETCD;

	/* Gestione ack ricevuti. */
	last_ack_rcvd = segwrap_ack_create (SEQMAX);
	ack_handled = TRUE;

#if !HAVE_MSG_NOSIGNAL
	/* Ignora SIGPIPE nei sistemi che non hanno MSG_NOSIGNAL. */
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
		fprintf (stderr, "Manca MSG_NOSIGNAL, tutti i SIGPIPE "
				"ignorati.\n");
	}
#endif
	return 0;

error:
	return -1;
}


bool
proxy_is_running (void)
{
	cd_t cd;

	for (cd = NETCD; cd < NETCD + NETCHANNELS; cd++)
		if (channel_is_connected (cd) 
		    || channel_is_activable (cd)
		    || channel_is_listening (cd)
		    || channel_is_connecting (cd))
			return TRUE;

	return FALSE;
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

	if (ch[cd].c_sockfd < 0)
		fprintf (stderr, "Canale %s, accept fallita: %s\n",
				channel_name (cd), strerror (errno));

	/* A prescindere dall'esito dell'accept, chiusura del socket
	 * listening. */
	tcp_close (&ch[cd].c_listfd);

	if (ch[cd].c_sockfd < 0)
		return -1;

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
			if (err) {
				perror ("connect_noblock");
				channel_close (i);
				continue;
			}

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
			if (err) {
				perror ("listen_noblock");
				channel_close (i);
				continue;
			}
			printf ("Canale %s in ascolto.\n", channel_name (i));
		}
		else
			assert (FALSE);
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


struct segwrap *
set_last_ack_rcvd (struct segwrap *ack)
{
	struct segwrap *old_ack;

	assert (last_ack_rcvd != NULL);
	if (segwrap_seqcmp (last_ack_rcvd, ack) < 0) {
		old_ack = last_ack_rcvd;
		last_ack_rcvd = ack;
		ack_handled = FALSE;
	} else
		old_ack = NULL;

	return old_ack;
}


void
urgent_add (struct segwrap *sw)
{
	cd_t destcd;

	assert (sw != NULL);

	/* Segmento duplicato, gia' assegnato a un canale. */
	if (segwrap_is_assigned (sw)) {
		assert (VALID_CD (sw->sw_assigned));
		destcd = sw->sw_assigned;
		if (channel_is_connected (destcd))
			qinorder_insert (&urgentq_priv[destcd], sw,
					&segwrap_urgcmp);
		else
			segwrap_destroy (sw);
	}
	/* Segmento da clonare e assegnare a ogni coda urgente dei canali con
	 * il ritardatore. */
	else if (segwrap_is_clonable (sw)) {
		for (destcd = NETCD; destcd < NETCD + NETCHANNELS; destcd++)
			if (channel_is_connected (destcd)) {
				sw->sw_assigned = destcd;
				qinorder_insert (&urgentq_priv[destcd],
						segwrap_clone (sw),
						&segwrap_urgcmp);
			}
		segwrap_destroy (sw);
	}
	/* Segmento standard, da accodare alla coda urgente generica. */
	else
		qinorder_insert (&urgentq, sw, segwrap_urgcmp);
}


struct segwrap *
urgent_head (cd_t cd)
{
	/* Ritorna un puntatore al segmento piu' urgente presente nelle code:
	 * prima controlla quella assegnata al canale specificato, se vuota
	 * controlla quella generica.
	 * Ritorna NULL se sono tutte vuote. */

	struct segwrap *head;

	assert (VALID_CD (cd));

	head = getHead (urgentq_priv[cd]);
	if (head == NULL)
		head = getHead (urgentq);
	return head;
}


struct segwrap *
urgent_remove (cd_t cd)
{
	/* Come urgent_head, in piu' rimuove il segwrap dalla coda che lo
	 * conteneva. */

	struct segwrap *rmvd;

	assert (VALID_CD (cd));

	rmvd = qdequeue (&urgentq_priv[cd]);
	if (rmvd == NULL)
		rmvd = qdequeue (&urgentq);
	return rmvd;
}


void
urgent_rm_acked (struct segwrap *ack)
{
	int cd;
	struct segwrap *rmvdq;

	assert (ack != NULL);

	for (cd = NETCD; cd < NETCD + NETCHANNELS; cd++) {
		rmvdq = qremove_all_that (&urgentq_priv[cd],
				&segwrap_is_acked, ack);
		while (!isEmpty (rmvdq))
			segwrap_destroy (qdequeue (&rmvdq));
	}
	rmvdq = qremove_all_that (&urgentq, &segwrap_is_acked, ack);
	while (!isEmpty (rmvdq))
		segwrap_destroy (qdequeue (&rmvdq));
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
host2net (void)
{
	int needmask;
	size_t host_nbytes;
	len_t pldlen;

	needmask = 0x7;
	host_nbytes = cqueue_get_used (host_rcvbuf);
	pldlen = MIN (host_nbytes, PLDDEFLEN);
	while (needmask != 0x0 && host_nbytes > 0) {
		if (channel_is_connected (rrcd)
		    && urgent_head (rrcd) == NULL
		    && rqueue_get_aval (net_sndbuf[rrcd]) >= (HDRMAXLEN
		                                              + pldlen)) {
			struct segwrap *newsw;

			newsw = segwrap_create ();
			segwrap_fill (newsw, host_rcvbuf, pldlen,
					outseq++);

			rqueue_add (net_sndbuf[rrcd], newsw);

			host_nbytes = cqueue_get_used (host_rcvbuf);
			pldlen = MIN (host_nbytes, PLDDEFLEN);
		} else
			needmask &= ~(0x1 << rrcd);
		ROTATE_RRCD;
	}
}


static void
urg2net (void)
{
	/* Trasferisce i segwrap dalla struttura dei segmenti urgenti ai
	 * buffer dei canali di rete in maniera round robin, finche' ci sono
	 * segmenti urgenti oppure i buffer si riempono. */

	int err;
	int needmask;
	cd_t cd;
	bool do_reorg;
	struct segwrap *sw;

	/* I net_sndbuf contengono i segmenti in ordine di urgenza. Se il piu'
	 * urgente della urgentq_priv e' piu' urgente dell'ultimo segmento di
	 * una qualsiasi rqueue bisogna inserirlo in mezzo: si travasano tutte
	 * le rqueue nelle code urgenti. */
	for (do_reorg = FALSE, cd = NETCD;
	     cd < NETCD + NETCHANNELS && !do_reorg;
	     cd++)
		if (channel_is_connected (cd)
		    && rqueue_get_used (net_sndbuf[cd]) > 0
		    && urgent_head (cd) != NULL
		    && segwrap_urgcmp (net_sndbuf[cd]->rq_sgmt,
			    urgent_head (cd)) > 0)
			do_reorg = TRUE;

	if (!do_reorg)
		goto transfer;

	/* Riorganizzazione buffer. */
	for (cd = NETCD; cd < NETCD + NETCHANNELS; cd++)
		if (channel_is_connected (cd)
		    && rqueue_get_used (net_sndbuf[cd]) > 0) {
			struct segwrap *unsentq;
			/* Taglio e travaso. */
			unsentq = rqueue_cut_unsent (net_sndbuf[cd]);
			while ((sw = qdequeue (&unsentq)) != NULL)
				urgent_add (sw);
		}

	/* Riempimento net_sndbuf. */
transfer:
	needmask = 0x7;
	while (needmask != 0x0) {
		if (channel_is_connected (rrcd)
		    && (sw = urgent_head (rrcd)) != NULL
		    && sw->sw_seglen <= rqueue_get_aval (net_sndbuf[rrcd])) {
			sw = urgent_remove (rrcd);
			assert (sw != NULL);
			err = rqueue_add (net_sndbuf[rrcd], sw);
			assert (!err);
		} else
			needmask &= ~(0x1 << rrcd);
		ROTATE_RRCD;
	}
}


static void
netsndbuf_rm_acked (struct segwrap *ack)
{
	cd_t cd;

	for (cd = NETCD; cd < NETCHANNELS; cd++)
		if (channel_is_connected (cd))
			rqueue_rm_acked (net_sndbuf[cd], ack);
}
