#include "h/types.h"
#include "h/segment.h"

#include <config.h>


#define     TYPE     struct segwrap
#define     NEXT     sw_next
#define     PREV     sw_prev
#define     EMPTYQ   NULL
#include "src/queue_template"


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static int hash_fun (seq_t key, size_t table_size);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

void
seghash_init (struct segwrap **hash_table, size_t table_size)
{
	int i;

	assert (hash_table != NULL);
	assert (table_size > 0);

	for (i = 0; i < table_size; i++)
		hash_table[i] = newQueue ();
}


void
seghash_add
(struct segwrap **hash_table, size_t table_size, struct segwrap *sw)
{
	int i;

	assert (hash_table != NULL);
	assert (table_size > 0);
	assert (sw != NULL);

	i = hash_fun (seg_seq (sw->sw_seg), table_size);
	assert (i >= 0);
	assert (i < table_size);

	qenqueue (&hash_table[i], sw);
}


struct segwrap *
seghash_get (struct segwrap **hash_table, size_t table_size, seq_t key)
{
	int i;
	struct segwrap *head;
	struct segwrap *cur;
	struct segwrap *selected;

	assert (hash_table != NULL);
	assert (table_size > 0);

	i = hash_fun (key, table_size);
	assert (i >= 0);
	assert (i < table_size);

	selected = NULL;
	head = getHead (hash_table[i]);
	if (head != NULL) {
		cur = head;
		do {
			if (key == seg_seq (cur->sw_seg))
				selected = cur;
			else
				cur = getNext (cur);
		} while (selected == NULL && cur != getHead (hash_table[i]));
	}

	return selected;

}


struct segwrap *
seghash_remove (struct segwrap **hash_table, size_t table_size, seq_t key)
{
	struct segwrap *selected;

	selected = seghash_get (hash_table, table_size, key);
	if (selected != NULL) {
		int i;
		i = hash_fun (key, table_size);
		qremove (&hash_table[i], selected);
	}
	return selected;
}


void
seghash_rm_acked
(struct segwrap **hash_table, size_t table_size, struct segwrap *ack)
{
	int i;
	struct segwrap *rmvdq;

	assert (hash_table != NULL);
	assert (table_size > 0);

	for (i = 0; i < table_size; i++) {
		rmvdq = qremove_all_that (&hash_table[i], &segwrap_is_acked,
				ack);
		while (!isEmpty (rmvdq))
			segwrap_destroy (qdequeue (&rmvdq));
	}
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

static int
hash_fun (seq_t key, size_t table_size)
{
	return (key % table_size);
}
