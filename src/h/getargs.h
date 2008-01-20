#ifndef GETARGS_H
#define GETARGS_H


/*******************************************************************************
				  Prototipi
*******************************************************************************/

int
getargs (int argc, char **argv, char *fmt, ...);
/* Secondo il formato fmt, converte i vari argv[i] in indirizzi o porte e li
 * scrive negli indirizzi dati.
 *
 * Ogni carattere della stringa fmt specifica o un indirizzo ip ('a') o una
 * porta ('p').
 *
 * Se il valore da linea di comando e' '-', lascia il valore di default.
 *
 * Ritorna TRUE se riesce, FALSE se fallisce. */


#endif /* GETARGS_H */
