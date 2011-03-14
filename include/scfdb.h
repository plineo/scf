#if !defined (_SCFDB_H_)
#define _SCFDB_H_

/* Numero maximo de ficheros que un cliente puede compartir */
#define MAX_FILES 100

/* Espacio que se reserva por defecto para almacenar clientes */
/* El espacio crece dinamicamente */
#define DEFAULT_CLIENTS 10

#include <netinet/in.h>
#include "mystringlib.h"

/* Estructura para almacenar la informacion de los clientes conectados. */
typedef struct t_client {
    char *ident;		// identificador o nick del cliente
    char *version;		// version del protocolo que usa
    struct in_addr ip;		// direccion IP
    unsigned short port;	// puerto de escucha	
    char *files[MAX_FILES];	// los ficheros que comparte
    char *file_size[MAX_FILES]; // con su tamano
    int nfiles;			// numero de ficheros que hay en files
    int pthread_id;		// identificador del hilo que atiende a este cliente
} CLI_ENTRY;

/* Inicializacion de las estructuras y variables. */
extern void init_scfdb (void);

/* Busca un cliente dado su IDENT. */
extern CLI_ENTRY *find_client (char *);

/* Inserta un nuevo cliente */
extern void add_client (char *, char *, struct in_addr, unsigned short, char *, int);

/* Limpiar la memoria */
extern void clear_clients (void);

/* Lista de clientes */
CLI_ENTRY **client_list (void);

/* Eliminar un cliente */
extern int remove_client (char *);

/* Lista los clientes conectados */
extern void print_clients_info(int);

/* Imprime los ficheros compartidos por un cliente */
extern void print_files(int);

/* Crear la lista de ficheros del cliente a partir de un string de ficheros */
void create_file_list(CLI_ENTRY *, char *);

void destroy_files(int);

/* Buscar ficheros */
filestruct *find_files (char *, int);

int clientcount(void);
#endif /* _SCFDB_H_ */

