/* util.c - funzioni di utilità generale.
 *
 * Giacomo Ritucci, 23/09/2007 */

#include "util.h"
#include "types.h"

#include <string.h>


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

bool
streq (const char *str1, const char *str2) {
	int cmp = strcmp (str1, str2);
	
	if (cmp == 0) return TRUE;
	return FALSE;
}
