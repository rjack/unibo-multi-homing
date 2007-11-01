#include "h/crono.h"
#include "h/types.h"

#include <assert.h>
#include <sys/time.h>


/*******************************************************************************
				 Definizioni
*******************************************************************************/

#define     ONE_MILLION     1000000


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static double tv_diff (const struct timeval *min, const struct timeval *sub);
static void tv_normalize (struct timeval *tv);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

/*
 * Cronometri.
 */

double
crono_measure (crono_t *cr) {
	struct timeval now;

	assert (cr != NULL);

	gettimeofday (&now, NULL);
	tv_normalize (&now);
	cr->cr_elapsed = tv_diff (&now, &cr->cr_start);

	assert (cr->cr_elapsed > 0);

	return crono_read (cr);
}


double
crono_read (crono_t *cr) {
	assert (cr != NULL);
	return cr->cr_elapsed;
}


void
crono_start (crono_t *cr) {
	assert (cr != NULL);

	cr->cr_elapsed = 0;
	gettime (&cr->cr_start);
	tv_normalize (&cr->cr_start);
}


/*
 * Strutture timeval.
 */

void
gettime (struct timeval *tv) {
	assert (tv != NULL);

	gettimeofday (tv, NULL);
	tv_normalize (tv);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

/*
 * Strutture timeval.
 */

static double
tv_diff (const struct timeval *min, const struct timeval *sub) {
	/* Sottrae sub da min, che si assumono essere normalizzate.
	 * In caso di differenza negativa ritorna 0. */

	/* Variabili d'appoggio per non modificare il minuendo. */
	time_t adjsec = 0;
	suseconds_t adjusec = 0;
	double result = 0;

	assert (min != NULL);
	assert (sub != NULL);

	/* De-normalizzazione. */
	if (min->tv_usec < sub->tv_usec) {
		adjsec = -1;
		adjusec = ONE_MILLION;
	}

	/* Calcolo in due passi: prima parte frazionaria, poi aggiunta di
	 * quella intera. */
	if (min->tv_sec >= sub->tv_sec) {
		result = (double)(min->tv_usec + adjusec - sub->tv_usec)
		         / (double)ONE_MILLION;
		result += (min->tv_sec + adjsec - sub->tv_sec);
	}

	return result;
}


static void
tv_normalize (struct timeval *tv) {

	assert (tv != NULL);

	if (tv->tv_usec >= ONE_MILLION) {
		tv->tv_sec += (tv->tv_usec / ONE_MILLION);
		tv->tv_usec = (tv->tv_usec % ONE_MILLION);
	}
}
