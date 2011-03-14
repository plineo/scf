#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <arpa/inet.h>
#include "mystringlib.h"
#include "scfdb.h"

/* Donde almacenamos los clientes conectados. */
CLI_ENTRY **clients = (CLI_ENTRY **)NULL;

/* Numero de clientes */
int client_count;

/* Tamano de los clientes.*/
int client_size;

/*###############################################################*/

/* Inicializacion */
void init_scfdb (void)
{
    client_size = DEFAULT_CLIENTS;
    clients = (CLI_ENTRY **)malloc (client_size * sizeof (CLI_ENTRY *));
    client_count = 1;
}

/* Devuelve una lista todos los clientes */
CLI_ENTRY **client_list (void)
{
    return (clients);
}

/* Busca un cliente por su IDENT, en caso de encontarlo, es devuelto */
/* si no es encontrado retorna NULL. */
CLI_ENTRY *find_client (char *name)
{
    CLI_ENTRY *temp;
    int i = 0, found = 1;

    if (client_count == 1)
	return ((CLI_ENTRY *)NULL);

    while ((i < client_count - 1) && (found != 0))
    {
        found = strcmp (clients[i]->ident, name);
	i++;
    }

    if (found == 0)
	{
	temp = (CLI_ENTRY *)malloc (sizeof (CLI_ENTRY));
	temp = clients[i - 1];
	return (temp);
	}
    else
    {

	return ((CLI_ENTRY *)NULL);
    }
}


/* Limpiar la memoria de clientes */
void clear_clients (void)
{
    register int i;
    for (i = 0; i < client_count - 1; i++)
    {
	free (clients[i]->ident);
	free (clients[i]->version);
	destroy_files(i);
	free (clients[i]);
	clients[i] = (CLI_ENTRY *)NULL;
    }
    free(clients);
    client_count = 0;
}


/* Inserta un nuevo cliente */
void add_client (char *name, char *version, struct in_addr ip, unsigned short port, char *files, int id)
{
    CLI_ENTRY *temp;

    temp = (CLI_ENTRY *)malloc(sizeof(CLI_ENTRY));	
    temp->ident = (char *)savestring(name);
    temp->version = (char *)savestring(version);
    temp->ip = ip;
    temp->port = port;
    
    if (files == NULL)
	temp->nfiles = 0;
    else
	create_file_list(temp, files);
    
    temp->pthread_id = id;

    /* si la memoria esta llena, reasignamos mas */
    if (client_count == client_size)
    {
        client_size += DEFAULT_CLIENTS;
	clients = (CLI_ENTRY **)realloc (clients, client_size * sizeof (CLI_ENTRY *));
    }

    clients[client_count] = (CLI_ENTRY *)NULL;
    clients[client_count - 1] = temp;
    client_count++;
}

/* Elimina un cliente */
int remove_client (char *name)
{
    int i = 0, found = 1, j;

    if (name == NULL)
    	return -1;

    if (client_count == 1)
	return -1;

    while ((i < client_count - 1) && (found != 0))
    {
        found = strcmp (clients[i]->ident, name);
	i++;
    }

    i--;

    if (found == 0)
    {
	free (clients[i]->ident);
	free (clients[i]->version);
	destroy_files(i);
	free (clients[i]);

	for(j = i;j < client_count - 1;j++)
	    clients[j] = clients[j+1];

	client_count--;
	return 0;
    }
    else
	return -1;
}

void print_clients_info(int index)
{
    int i;
    
    if (index == -1)
    {
	for (i = 0; i < client_count - 1; i++)
	    printf("[%d]  \033[1;31m%s\n\033[0;39m", i+1, clients[i]->ident);
	if ((client_count - 1) == 0)
	    printf("No connections.\n");
	return;
    }
    
    i = index - 1;
    
    if ((i < 0) || (i >= client_count - 1))
    {
	printf("Unknown id.\n");
	return;
    }
    else
    {
	printf("\nident:\t%s\n", clients[i]->ident);
	printf("ver:\t%s\n", clients[i]->version);
	printf("IP:\t%s\n", inet_ntoa(clients[i]->ip));
	printf("port:\t%d\n", clients[i]->port);
	printf("thread:\t%d\n", clients[i]->pthread_id);
	printf("files:\t%d\n", clients[i]->nfiles);
    }	

}

void create_file_list(CLI_ENTRY *c, char *buffer)
{
    int i = 0;
    
    while ((*buffer != '\0') && (i < MAX_FILES))
    {
	c->files[i] = (char *)savestring(buffer);
	buffer += strlen(buffer)+1;
	c->file_size[i] = (char *)savestring(buffer);
	buffer += strlen(buffer)+1;
	i++;
    }
    c->nfiles = i;
}

void destroy_files (int client)
{
    int i;
    for (i = 0;i < clients[client]->nfiles; i++)
    {
	free(clients[client]->files[i]);
	free(clients[client]->file_size[i]);
    }
}

void print_files (int client)
{
    CLI_ENTRY *c;
    int i;
    int j = 0;
    
    i = client - 1;
    
    if ((i < 0) || (i >= client_count - 1))
    {
	printf("Unknown id.\n");
	return;
    }
    else
    {
	c = clients[i];
	printf("\nFilename\033[45GSize (bytes)\n");
	
	for (j = 0; j < c->nfiles; j++)
	    printf("\033[1;33m%s\033[45G%s\n\033[0;39m", c->files[j], c->file_size[j]);
	printf("\n%s sharing %d files.\n", c->ident, c->nfiles);
    }
}

/* 
 Busca ficheros en la base de datos que correspondan al patron pattern
 id es el identificador del hilo que esta atendiendo esta consulta,
 si encontramos en la bd un identificador asociado a un cliente igual al
 dado, no enviamos estos ficheros, ya que le pertenecen y creemos que no es
 necesario enviar a un cliente, sus propios ficheros.
 
 Para realizar el chequeo del patron, usamos la llamada fnmatch() de C
 
 */
 
filestruct *find_files(char *pattern, int id)
{
    int i, j, k, buffer_size = 500, len = 0;
    filestruct *f;
    char *aux;
    
    f = (filestruct *)malloc(sizeof(filestruct));
    f->files = (char *)malloc(buffer_size);
    aux = f->files;

    for (i = 0;i < client_count - 1;i++)
    {
	if(id == clients[i]->pthread_id)
	    continue;

	for (j = 0;j< clients[i]->nfiles;j++)
	{
	    k = fnmatch(pattern, clients[i]->files[j], 0);
	    if (k == 0)
	    {
		len += strlen(clients[i]->files[j]) + strlen(clients[i]->file_size[j]) + strlen(clients[i]->ident) + 3;
		
		if (len >= buffer_size)
		{
		    buffer_size += 500;
		    aux = (char *)realloc (aux, buffer_size);
		}
		//nombre
		strcpy(f->files, clients[i]->files[j]);
		f->files += strlen(f->files) + 1;
		//tamano
		strcpy(f->files, clients[i]->file_size[j]);
		f->files += strlen(f->files) + 1;
		//identidad
		strcpy(f->files, clients[i]->ident);
		f->files += strlen(f->files) + 1;
	    }		
	}
    }
    f->lfiles = len;
    f->files = aux;
    
    return (f);
}

int clientcount(void)
{
    return (client_count - 1);
}
