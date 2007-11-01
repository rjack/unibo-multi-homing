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


#endif /* CRONO_H */
