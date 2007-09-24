/* types.h - definizioni dei tipi per tutto il progetto.
 *
 * Giacomo Ritucci, 23/09/2007 */

#ifndef MH_TYPES_H
#define MH_TYPES_H


/* Booleani e relativi valori. */

typedef unsigned char bool;

#ifdef TRUE
#undef TRUE
#endif
#define     TRUE     ((bool)1)

#ifdef FALSE
#undef FALSE
#endif
#define     FALSE     ((bool)0)


#endif /* MH_TYPES_H */
