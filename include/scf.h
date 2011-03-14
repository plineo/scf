#if !defined (_SCF_H_)
#define _SCF_H

#define PORT 9000
#define VERSION "0.2"
#define MAX_FILES 100
#define SHARED_FOLDER_IN "/scf/entrada/"
#define SHARED_FOLDER_OUT "/scf/salida/"
#define BLOCK_SIZE 4096

#include "scfdb.h"

typedef enum { EC, LF, CON, PDC, DC, PF, DF, FC, ACK, ERR } pkg_type;

typedef unsigned short t_type;		// 2 bytes
typedef unsigned int t_len;		// 4 bytes
typedef unsigned char t_char;
typedef unsigned short t_port;		// 2 bytes
typedef unsigned int t_ip;		// 4 bytes

#define TYPE_SIZE sizeof(t_type)
#define LEN_SIZE sizeof(t_len)
#define CHAR_SIZE sizeof(t_char)
#define PORT_SIZE sizeof(t_port)
#define IP_SIZE sizeof(t_ip)


//Las estructuras de los paquetes LF, CON, PDC, PF, DF, ACK y ERR 
typedef struct default_pkg
{
    t_type type;
    t_len len;
    char *data;
} default_pkg;

// Estructuras de paquetes especiales
typedef struct ec_pkg {
    default_pkg header;
    t_port port;
    char *ident;
    char *ver;
} ec_pkg;

typedef struct dc_pkg {
    default_pkg header;
    struct in_addr ip;
    t_port port;
    char *version;
} dc_pkg;


/* Tamano de las cabeceras, es decir, sin incluir la parte de datos variable, */
/* solo los campos que siempre son fijos en los paquetes */

#define DEFAULT_HEADER_SIZE (TYPE_SIZE + LEN_SIZE)
#define EC_HEADER_SIZE (DEFAULT_HEADER_SIZE + PORT_SIZE)
#define DC_HEADER_SIZE (DEFAULT_HEADER_SIZE + IP_SIZE + PORT_SIZE)

/****************** Rutinas de envio de datos *********************************/

extern int send_default_pkg(int, int, char *, pkg_type);

extern int send_ec_pkg(int, char *, int);

extern int send_dc_pkg(int, CLI_ENTRY *);

extern int send_fc_pkg(int);

/****************** Rutinas de recepcion de datos *********************************/

extern int recv_default_pkg(int, default_pkg *);

extern int recv_ack_err(int);

extern int recv_ec_pkg(int, ec_pkg *);

extern int recv_dc_pkg(int, dc_pkg *);




#endif

