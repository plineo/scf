#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "scf_client.h"

/* Buffers del programa y punteros de trabajo. */
static char inpbuf[MAXBUF], tokbuf[2*MAXBUF], *ptr=inpbuf, *tok=tokbuf;
static char special[] = {' ','\t',':','\n','\0'};

void userin(void) {

   /* Inicializacion para rutinas posteriores. */
    ptr = inpbuf;
    tok = tokbuf;

    printf ("# ");
    fflush(stdout);
    fgets(ptr, MAXBUF-1, stdin);

}

/* Comprueba si el caracter que se el pasa como parametro es especial o no,*
 * devolviendo 0 si es asi o 1 en caso contrario. */
 
int inarg(char c) {
   char *wrk;
   
   for(wrk = special; *wrk != '\0'; wrk++)
     if (c == *wrk)
        return(0);
   
   return(1);
}

/* Lee un token (palabra) y lo coloca en tokbuf. ptr apunta en todo momento *
 * el siguiente caracter de la linea de entrada a procesar. tok contendra el*
 * siguiente token al terminar la funcion, que devuelve el tipo de token    *
 * leido. */
int gettok(char **outptr) {
   int type;
   
   *outptr = tok;
   
   /* Saltamos los espacios en blanco y tabuladores. */
   for(;*ptr == ' ' || *ptr == '\t'; ptr++);
   
   *tok++ = *ptr;
   
   switch(*ptr++) {
    case '\n':
      type = EOL; break;
    case ':':
      type = DDOT; break;
    default:
      type = ARG;
      /* Mientras no encontremos un caracter especial, almacenamos en tok el *
       * siguiente token. */
      while(inarg(*ptr))
	*tok++ = *ptr++;
   }
   
   *tok++ = '\0';
   return(type);
}


/* El siguiente procedimiento procesa una linea de entrada. La ejecucion *
 * termina cuando se encuentra un final de linea ('\n').  */

void procline(void)
{
   char *arg[MAXARG+1];  
   int toktype;          
   int narg;              
   int n;

   for(narg = 0;;) {
      switch(toktype = gettok(&arg[narg])) 
      {
      
	case ARG:  	 		
           if (narg < MAXARG)
		narg++;
 	    break;
         
     default:
	if (narg != 0)
	{
	    arg[narg] = NULL;
	    n = run_command (arg);
	    //Si no es un comando aceptado por el cliente, se lo mandamos al shell
	    if (n == -1)
		system((char *)&inpbuf);
	}
	 
	 if (toktype == EOL)   /* Terminar si se ha leido un final de línea. */
	   return;
	 
	 break;
      }
   }
    

}

/* Ejecuta un comando del servidor */
int run_command (char **cline)
{
    char *commands[] = {"open","logout", "get", "list", "whoami", "whois", "mode", "quit", "ident", "help"};
    int i = 0, aux = 1;
    
    while ((i < MAX_CLIENT_COMMANDS) && (aux != 0))
    {
	aux = strcmp (cline[0], commands[i]);
	i++;
    }
    
    if (aux == 0)
	switch (i)
	{
	    case 1: 
		    do_connect(cline[1]);
		    break;
		
	    case 2:
		    do_logout();
		    break;
	    
	    case 3:
	    	    if (cline[1] == NULL)
			printf("file id required!\n");
		    else
			get_file(atoi(cline[1]));
		    break;
		    
            case 4:
		    if (cline[1] == NULL)
			printf("pattern required!\n");
		    else
			get_list(cline[1]);
            	    break;
		    
	    case 5:
		    print_info();
		    break;
		    
	    case 6:
		    if (cline[1] == NULL)
			printf("Ident required!\n");
		    else
			do_whois(cline[1]);
		    break;
		    
	    case 7:
		    if (cline[1] == NULL)
			set_mode(NULL);
		    else
			set_mode(cline[1]);
		    break;

	    case 8:
		    terminate_client();
		    break;

	    case 9:
		    set_ident(cline[1]);
		    break;
	    case 10:
		    print_help();
		    break;
		    
	}
        else
	    aux = -1;

    return (aux);
}

void print_help(void)
{
    printf("Commands are:\n\n");
    printf("open hostname\t\topen a SCF Server site.\n");
    printf("logout\t\t\tlogout from server.\n");
    printf("get id\t\t\tget the file id.\n");
    printf("list pattern\t\tget a list of files from server.\n");
    printf("quit\t\t\tclose the client.\n");
    printf("mode [text/bin]\t\tset the file transfer mode.\n");
    printf("whoami\t\t\tshow information about yourself.\n");
    printf("whois ident\t\tshow information about ident.\n");
    printf("ident [name]\t\tset/show your nick or ident.\n");
    printf("help\t\t\tprint help information.\n\n");
}

