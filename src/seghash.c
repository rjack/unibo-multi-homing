#include <config.h>
#include <assert.h>

#define     TYPE     struct segwrap
#define     NEXT     sw_next
#define     PREV     sw_prev
#define     EMPTYQ   NULL
#include "src/queue_template"


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
seghash_init (struct segwrap **hash_table, size_t table_size)
{
	/* TODO seghash_init */
}


void
seghash_add (struct segwrap **hash_table, struct segwrap *sw)
{
	/* TODO seghash_add */
}


struct segwrap *
seghash_remove (struct segwrap **hash_table, seq_t key)
{
	/* TODO seghash_remove */
}
