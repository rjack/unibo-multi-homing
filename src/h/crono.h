#ifndef CRONO_H
#define CRONO_H

#include "types.h"


/*******************************************************************************
				  Prototipi
*******************************************************************************/

/*
 * Cronometri.
 */

double
crono_measure (crono_t *cr);


double
crono_read (crono_t *cr);


void
crono_start (crono_t *cr);


/*
 * Strutture timeval.
 */

void
gettime (struct timeval *tv);


void
d2tv (double value, struct timeval *tv);


double
tv2d (struct timeval *tv, bool must_free);

#endif /* CRONO_H */
