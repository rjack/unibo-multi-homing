#include "h/cqueue.h"
#include "h/types.h"
#include "h/util.h"

#include <assert.h>
#include <config.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

int
rqueue_add (rqueue_t *rq, seg_t *seg, size_t seglen)
{
	return -1;
}


void
rqueue_add_nak (rqueue_t *rq, seq_t seq)
{
}


bool
rqueue_can_read (void *arg)
{
	return FALSE;
}


bool
rqueue_can_write (void *arg)
{
	return FALSE;
}


rqueue_t *
rqueue_create (void)
{
	return NULL;
}


void
rqueue_destroy (rqueue_t *rq)
{
}


size_t
rqueue_get_aval (rqueue_t *rq)
{
	return 0;
}


size_t
rqueue_get_used (rqueue_t *rq)
{
	return 0;
}


int
rqueue_read (fd_t fd, rqueue_t *rq)
{
	return 0;
}


seg_t *
rqueue_remove (rqueue_t *rq)
{
	return -1;
}


int
rqueue_write (fd_t fd, rqueue_t *rq)
{
	return 0;
}
