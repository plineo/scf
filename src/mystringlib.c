#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mystringlib.h"

/* Almacena la cadena S en memoria */
char *savestring (char *s)
{
    return ((char *)strcpy(malloc (1+(int)strlen(s)),(s)));
}

/* Crea una nueva cadena a partir de STRING comenzando en START y
   terminando en END, sin incluir END. */
char *substring (string, start, end)
     char *string;
     int start, end;
{
  register int len;
  register char *result;

  len = end - start;
  result = (char *) calloc (len + 1, sizeof(char));
  strncpy (result, string + start, len);
  result[len] = '\0';
  return (result);
}

/* Cuenta el numero de apariciones del caracter C, en STRING. */
int nchars (char *string, int c)
{
    int i, len, nc = 0;
    len = strlen (string) - 1;
    for (i = 0; i < len; i++)
    	if (string[i] == c)
        	nc++;
	return (nc);
}

/* Devuelve la posicion relativa del primer caracter C, en la cadena STRING. */
int posc (char *string, int c)
{
    int i = 0, len, pos = 0;
    len = strlen (string) - 1;
    while ((pos == 0) && (i < len))
    {
    	if (string[i] == c)
        	pos = i;
		i++;
    }
    return (pos);
}
