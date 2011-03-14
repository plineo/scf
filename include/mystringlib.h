#if !defined (_MYSTRINGLIB_H_)
#define _MYSTRINGLIB_H_

typedef struct filestruct
{
    int nfiles;
    int lfiles;
    char *files;
} filestruct;

/* Almacena una cadena, es decir, se reserva el espacio justo. */
extern char *savestring (char *);

/* crea una subcadena de una cadena dada */
/* util para extraer palabras o frases de una cadena */
extern char *substring (char *, int, int);

/* Numero de apariciones de un determinado caracter en una cadena */
extern int nchars (char *, int);

/* posicion relativa de la primera aparicion de un caracter dado. */
extern int posc (char *, int);

#endif
