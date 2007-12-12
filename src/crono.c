#include "h/crono.h"
#include "h/types.h"
#include "h/util.h"

#include <config.h>
#include <assert.h>
#include <sys/time.h>


/*******************************************************************************
				 Definizioni
*******************************************************************************/

#define     ONE_MILLION     1000000


/*******************************************************************************
		       Prototipi delle funzioni locali
*******************************************************************************/

static double tv_diff (struct timeval *min, struct timeval *sub);
static bool tv_is_normalized (struct timeval *tv);
static void tv_normalize (struct timeval *tv);


/*******************************************************************************
			      Funzioni pubbliche
*******************************************************************************/

/*
 * Cronometri.
 */

double
crono_measure (crono_t *cr)
{
	struct timeval now;

	assert (cr != NULL);

	gettimeofday (&now, NULL);
	tv_normalize (&now);
	cr->cr_elapsed = tv_diff (&now, &cr->cr_start);

	assert (cr->cr_elapsed > 0);

	return crono_read (cr);
}


double
crono_read (crono_t *cr)
{
	assert (cr != NULL);
	return cr->cr_elapsed;
}


void
crono_start (crono_t *cr)
{
	assert (cr != NULL);

	cr->cr_elapsed = 0;
	gettime (&cr->cr_start);
	tv_normalize (&cr->cr_start);
}


/*
 * Strutture timeval.
 */

void
gettime (struct timeval *tv)
{
	assert (tv != NULL);

	gettimeofday (tv, NULL);
	tv_normalize (tv);
}


void
d2tv (double value, struct timeval *tv)
{
	assert (value >= 0);
	assert (tv != NULL);

	tv->tv_sec = 0;
	while (value > 1) {
		value--;
		tv->tv_sec++;
	}
	tv->tv_usec = value * ONE_MILLION;

	assert (tv_is_normalized (tv));
}


double
tv2d (struct timeval *tv, bool must_free)
{
	double result;

	assert (tv != NULL);
	assert (must_free == TRUE || must_free == FALSE);

	result = tv->tv_sec + (double)tv->tv_usec / (double)ONE_MILLION;

	if (must_free) {
		xfree (tv);
	}

	return result;
}


/*
 * Timeout.
 */

double
timeout_left (timeout_t *to)
{
	assert (to != NULL);
	return (to->to_maxval - crono_measure (&to->to_crono));
}


void
timeout_set (timeout_t *to, double value)
{
	assert (to != NULL);
	assert (value > 0);

	to->to_maxval = value;
	crono_start (&to->to_crono);
}


/*******************************************************************************
			       Funzioni locali
*******************************************************************************/

/*
 * Strutture timeval.
 */

static double
tv_diff (struct timeval *min, struct timeval *sub)
{
	double dmin;
	double dsub;

	dmin = tv2d (min, FALSE);
	dsub = tv2d (sub, FALSE);

	return (dmin > dsub ? dmin - dsub : 0);
}


static bool
tv_is_normalized (struct timeval *tv)
{
	assert (tv != NULL);

	return (tv->tv_usec < ONE_MILLION ? TRUE : FALSE);
}


static void
tv_normalize (struct timeval *tv)
{
	assert (tv != NULL);

	if (tv->tv_usec >= ONE_MILLION) {
		tv->tv_sec += (tv->tv_usec / ONE_MILLION);
		tv->tv_usec = (tv->tv_usec % ONE_MILLION);
	}
}
