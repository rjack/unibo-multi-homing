#include "h/util.h"
#include "h/types.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static size_t buffer_get_available (struct buffer *buf);
static int buffer_grant_available (struct buffer *buf, size_t needed);
static int buffer_realloc (struct buffer *buf, size_t new_len);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
buffer_destroy (struct buffer *buf) {
	assert (buf != NULL);
	xfree (&buf->b_data);
}


void
buffer_init (struct buffer *buf, size_t len, bool resizable) {
	assert (buf != NULL);
	assert (len > 0);
	assert (BOOL_VALUE (resizable));

	buf->b_data = xmalloc (len);
	buf->b_len = len;
	buf->b_used = 0;
	buf->b_resizable = resizable;
}


int
buffer_read (fd_t fd, struct buffer *buf) {
	/* Legge da fd e salva in buf. */

	int err;
	size_t bytes_to_read;
	size_t available_bytes;
	ssize_t nread;

	assert (fd >= 0);
	assert (buf != NULL);
	assert (BOOL_VALUE (buf->b_resizable));

	available_bytes = buffer_get_available (buf);

	if (buf->b_resizable == TRUE) {
		bytes_to_read = tcp_get_buffer_size (fd, SO_RCVBUF);
		err = buffer_grant_available (buf, bytes_to_read);
		if (err) {
			bytes_to_read = available_bytes;
		}
	} else {
		bytes_to_read = available_bytes;
	}
	/* fd non deve essere impostato in lettura se non c'e' spazio nei
	 * buffer. */
	assert (bytes_to_read > 0);

	do {
		nread = read (fd, &(buf->b_data[buf->b_used]), bytes_to_read);
	} while (nread < 0 && errno == EINTR);

	if (nread > 0) {
		buf->b_used += nread;
	}

	return nread;
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static size_t
buffer_get_available (struct buffer *buf) {
	assert (buf != NULL);
	assert (buf->b_used <= buf->b_len);

	return (buf->b_len - buf->b_used);
}


static int
buffer_realloc (struct buffer *buf, size_t new_len) {
	char *new_data;

	assert (buf != NULL);
	assert (buf->b_resizable == TRUE);
	assert (new_len > 0);

	new_data = realloc (buf->b_data, new_len);
	if (new_data != NULL) {
		buf->b_data = new_data;
		buf->b_len = new_len;

		return 0;
	}
	return -1;
}


static int
buffer_grant_available (struct buffer *buf, size_t needed) {
	/* Cerca di garantire che buf abbia almeno needed byte liberi nel
	 * buffer, riallocando se necessario.
	 *
	 * Ritorna 0 se riesce, -1 se fallisce. */

	size_t available_bytes;

	assert (buf != NULL);
	assert (buf->b_resizable == TRUE);
	assert (needed > 0);

	available_bytes = buffer_get_available (buf);
	if (available_bytes < needed) {
		return buffer_realloc (buf, buf->b_len +
		                       (needed - available_bytes));
	}
	return 0;
}
