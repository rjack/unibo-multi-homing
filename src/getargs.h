/* getargs.h - lettura argomenti da linea di comando.
 *
 * Giacomo Ritucci, 23/09/2007 */

#ifndef GETARGS_H
#define GETARGS_H


#include "types.h"


bool
getargs (int argc, char **argv, char *fmt, ...);

#endif /* GETARGS_H */
