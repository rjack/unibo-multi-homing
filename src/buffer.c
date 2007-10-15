#include "h/util.h"
#include "h/types.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

size_t
buffer_available (struct buffer *buf) {
	assert (buf != NULL);
	assert (buf->b_used <= buf->b_len);
	
	return (buf->b_len - buf->b_used);
}


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

	size_t bytes_to_read;
	ssize_t nread;

	assert (fd >= 0);
	assert (buf != NULL);
	assert (BOOL_VALUE (buf->b_resizable));

	if (buf->b_resizable == TRUE) {
		/* TODO
		 bytes_to_read = tcp_get_buffer_len (fd);
		 */
		assert (FALSE);
		if (buffer_available (buf) < bytes_to_read) {
			/* TODO
			buffer_realloc (bytes_to_read);
			*/
			assert (FALSE);
		}
	} else {
		bytes_to_read = buffer_available (buf);
	}
	assert (bytes_to_read > 0);

	do {
		nread = read (fd, &(buf->b_data[buf->b_used]), bytes_to_read);
	} while (nread < 0 && errno == EINTR);

	/* TODO
	 * controllo esito
	 * manutenzione contatori */

	return -1;
}
