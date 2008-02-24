#ifndef SEGHASH_H
#define SEGHASH_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

void
seghash_init (struct segwrap **hash_table, size_t table_size);


void
seghash_add
(struct segwrap **hash_table, size_t table_size, struct segwrap *sw);


struct segwrap *
seghash_remove (struct segwrap **hash_table, size_t table_size, seq_t key);


void
seghash_rm_acked
(struct segwrap **hash_table, size_t table_size, struct segwrap *ack);

#endif /* SEGHASH_H */
