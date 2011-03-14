#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "scfdb.h"
#include "scf_server.h"

/* Buffers del programa y punteros de trabajo. */
static char inpbuf[MAXBUF], tokbuf[2*MAXBUF], *ptr=inpbuf, *tok=tokbuf;
static char special[] = {' ','\t',':','\n','\0'};

void userin(void) {

   /* Inicializacion para rutinas posteriores. */
    ptr = inpbuf;
    tok = tokbuf;
   
    /* prompt del shell */
    printf ("\n> ");
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
	    run_command (arg);
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
    char *commands[] = {"whoami","info", "users", "shutdown", "help", "files"};
    int i = 0, aux = 1;
    
    while ((i < MAX_SERVER_COMMANDS) && (aux != 0))
    {
	aux = strcmp (cline[0], commands[i]);
	i++;
    }
    if (aux == 0)
	switch (i)
	{
	    case 1: 
		    print_server_info();
		    break;
		    
	    case 2:
		    if (cline[1] == NULL)
			printf("Id required.\n");
		    else
			print_clients_info(atoi(cline[1]));
		    break;		
		    
	    case 3:
            	    print_clients_info(-1);
		    break;
	    
	    case 4:
		    shutdown_server(cline[1]);
		    break;
		    
            case 5:
		    print_help();
                    break;
	    case 6:
	    	    if (cline[1] == NULL)
			printf("Id required.\n");
		    else
			print_files(atoi(cline[1]));
		    break;
	}
        else
            printf("%s: Unknown command. Type 'help' for a list of commands.\n", cline[0]);
    return (aux);
}

void print_help(void)
{
    printf("Commands are:\n\n");
    printf("whoami\t\t\tshow information about current SCF Server.\n");
    printf("info id\t\t\tshow information about client id.\n");
    printf("users\t\t\tview all current connections.\n");
    printf("files id\t\tlist all shared files by client id.\n");
    printf("shutdown [now/wait]\tshutdown the SCF Server.\n");
    printf("help\t\t\tprint help information.\n");
}

