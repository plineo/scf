/*
    Protocolo SCF version 0.1
    scf.c: Rutinas de envio y recepcion de datos para el
	   protocolo SCF v.0.1
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include "mystringlib.h"
#include "scf.h"
#include "scfdb.h"

/**************** Rutinas de ENVIO de datos ****************************/

/* 
 Rutina para enviar un paquete simple, TIPO + LEN + DATOS.
 
 socket: descriptor de socket, debe estar abierto antes de la llamada
 ldata:	 longitud en bytes de los datos que hay en data
 data:   parte de datos 
 pkg_t:  tipo de paquete: CON, PDC, PF, DF, ACK Y ERR 
 
 Esta rutina puede ser usada por el cliente y el servidor

 Valor de retorno: bytes enviados o -1 si se produjo un error

*/
 
int send_default_pkg(int socket, int ldata, char *data, pkg_type p_type)
{
    int n = 0, m = 0;
    t_len *len;
    t_type *type;
    char *buf = (char *)NULL;
    
    buf = (char *)malloc(DEFAULT_HEADER_SIZE);

    type = (t_type *)buf;
    len = (t_len *)(buf+2);

    *type = htons(p_type);
    *len = htonl(ldata);

    n = send(socket, buf, DEFAULT_HEADER_SIZE, 0);

    if (n == -1)
    {
	free(buf);
	return -1;
    }
    
    /* Si hay datos a enviar, se envian */
    if (ldata > 0)
    {
	m = send(socket, data, ldata, 0);
	if (m == -1)
	{
	    free(buf);
	    return -1;
	}
	n += m;
    }
    free(buf);
    return n;
}

/* 
 Rutina para enviar un paquete EC. 
 Uso exclusivo del cliente.
 
 socket: descriptor de socket, debe estar abierto antes de la llamada
 ident:  identificador del cliente, debe ser una cadena terminada en '\0' 
 aport:  puerto de escucha del cliente. 
 
 La version del protocolo, es una constante definida en VERSION.

 Valor de retorno: 
     byteas enviados o
    -1 si hubo algun error 
    
*/
    
int send_ec_pkg (int socket, char *ident, int aport)
{
    char *buf = (char *)NULL;
    char *aux;
    int n, l;
    t_len *len;
    t_type *type;
    t_port *port;
             
    l = EC_HEADER_SIZE + strlen(ident) + strlen(VERSION) + 2;
    buf = (char *)malloc(l);
    aux = buf;

    type = (t_type *)buf;
    len = (t_len *)(buf+2);
    port = (t_port *)(buf+6);
    
    *type = htons(EC);
    *len = htonl(l - DEFAULT_HEADER_SIZE);
    *port = htons(aport);
    
    buf += EC_HEADER_SIZE;
    
    strcpy(buf,ident);
    buf += strlen(ident)+1;
    strcpy(buf,VERSION);
         
    n = send(socket, aux, l, 0);
    free(aux);
    return n;
}


/* 
 Rutina para enviar un paquete DC. 
 Uso exclusivo del servidor.
 Usada para enviar informacion sobre un cliente a otro cliente.
 
 socket: descriptor de socket, debe estar abierto antes de la llamada
 cli:   estructura con informacion sobre el cliente. 
*/

int send_dc_pkg(int socket, CLI_ENTRY *cli)
{
    char *buf = (char *)NULL;
    char *aux;
    int n, l;
    t_len *len;
    t_type *type;
    t_port *port;
    t_ip *ip;

    l = DC_HEADER_SIZE + strlen(cli->version) + 1;
    buf = (char *)malloc(l);

    type = (t_type *)buf;
    len = (t_len *)(buf+2);
    ip = (t_ip *)(buf+6);
    port = (t_port *)(buf+10);

    aux = buf;
    
    *type = htons(DC);
    *len = htonl(l - DEFAULT_HEADER_SIZE);
    *ip = cli->ip.s_addr;
    *port = htons(cli->port);
    
    buf += DC_HEADER_SIZE;
    strcpy(buf,cli->version);

    n = send(socket, aux, l, 0);
    free(aux);
    
    return n;
}

/* Rutina para enviar un paquete FC, Fin de Conexion */
int send_fc_pkg(int socket)
{
    int n = 0;
    t_type t;
    t = htons(FC);

    n = send(socket, &t, TYPE_SIZE, 0);
    
    return n;
}


/***************************************************************************/
/**************** Rutinas de RECEPCION de datos ****************************/
/***************************************************************************/

/*
 Rutina para recibir un paquete simple o por defecto, esto es TIPO + LEN + DATA
 
 Valor de retorno:
 -1: si hay algun error o si se corta la comunicacion
  0: si todo es correcto
  
*/
int recv_default_pkg(int socket, default_pkg *p)
{
    t_len *len;
    t_type *type;
    t_type t;
    int n;
    char *buf = (char *)NULL;
    
    /* primera lectura del canal para ver si es un paquete FC */    
    n = recv(socket, &t, TYPE_SIZE, MSG_PEEK);
    
    if (n == -1)
	return -1;

    t = ntohs(t);

    /* si es un paquete FC, ya hemos terminado */
    if (t == FC)
    {
	p->type = t;
	p->len = 0;
	p->data = (char *)NULL;
	return 0;
    }

    buf = (char *)malloc(DEFAULT_HEADER_SIZE);
    type = (t_type *)buf;
    len = (t_len *)(buf+2);

    /* Si no es una FC, hacemos segunda lectura del canal */
    n = recv(socket, buf, DEFAULT_HEADER_SIZE, MSG_WAITALL);

    /* La comunicacion falla */
    if (n == -1)
    {
	free(buf);
	return -1;
    }

    p->type = ntohs(*type);
    p->len = ntohl(*len);    

    /* Si hay datos, tenemos que hacer una tercera y ultima lectura */
    if (p->len > 0)
    {
	p->data = (char *)malloc((p->len)+1);
	if (p->data == NULL)
	{
	    free(buf);
	    return -1;
	}
	n = recv(socket, p->data, p->len, MSG_WAITALL);
	//la comunicacion falla
	if (n == -1)
	{
	    free(buf);
	    free(p->data);
	    return -1;
	}
	p->data[p->len] = '\0';
    }
    else
	p->data = (char *)NULL;
    
    free(buf);
    return 0;
}

/* Rutina para recibir un paquete ACK o ERR */
/* deberia devolver 0 si ACK, 1 si ERR (paquete ERR), y -1 si hubo un error */
int recv_ack_err(int socket)
{
    int n;
    default_pkg ack;    
    
    n = recv_default_pkg(socket, &ack);        
    
    if (n == -1)
	return -1;
    
    switch (ack.type)
    {
	case ACK:
	    n = 0;
	    break;
	
	case ERR:
	    n = 1;
	    break;
	    	
	default:
	    n = -1;
	    break;
    }
    
    /* si el paquete trae un mensaje se imprime */
    if (ack.len > 0)
	printf("\033[1;37m%s\n\033[0;39m", ack.data);

    return n;
}

/*
 Rutina para recibir un paquete EC
 La informacion recibida queda almacenda en una estructura
 de tipo ec_pkg
 
 Valor de retorno:
 
 -1: si hay algun error o se corta la comunicacion durante la recepcion
  0: si todo va bien
  1: si se recibe un paquete FC
  2: si el paquete no es el esperado
*/
int recv_ec_pkg(int socket, ec_pkg *ec)
{
    int n;
    t_len *len;
    t_type *type;
    t_type t;
    t_port *port;
    char *buf = (char *)NULL;
    char *data = (char *)NULL;

    /* Primero hacemos una lectura sin eliminar el contendio del
    canal, para ver el tipo del paquet */
    n = recv(socket, &t, TYPE_SIZE, MSG_PEEK);
    
    if (n == -1)
	return -1;

    t = ntohs(t);

    if (t == FC)
    {
	ec->header.type = t;
	ec->header.len = 0;
	return 1;
    }

    buf = (char *)malloc(EC_HEADER_SIZE);

    /* Esperamos a que nos lleguen todos los bytes 
    que sabemos son fijos en un paquete EC, esto es 10 */
    n = recv(socket, buf, EC_HEADER_SIZE, MSG_WAITALL);

    //La comucacion ha fallado
    if (n == -1)
    {
	free(buf);
	return -1;
    }
    
    type = (t_type *)buf;
    len = (t_len *)(buf+2);
    port = (t_port *)(buf+6);
    
    ec->header.type = ntohs(*type);
    ec->header.len = ntohl(*len);
    ec->port = ntohs(*port);

    //Se recibe un paquete FC, finalizacion de conexion
    if (ec->header.type == FC)
    {
	free(buf);
	return 1;
    }
    
    //Se ha recibido un paquete inesperado
    if (ec->header.type != EC)
    {
	free(buf);
	return 2;
    }

    data = (char *)malloc(ec->header.len);
    if (data == NULL)
    {
	free(buf);
	return -1;
    }
    n = recv(socket, data, ec->header.len, 0);
    
    if (n == -1)
    {
	free(buf);
	return -1;
    }
    
    ec->ident = (char *)savestring(data);
    ec->ver = (char *)savestring(data + strlen(data) + 1);

    free(buf);
    free(data);
    
    return 0;
}

/*
 Rutina para recibir un paquete DC
 
 Valor de retorno:
  0: si todo va bien
 -1: si hay algun error
*/

int recv_dc_pkg(int socket, dc_pkg *dc)
{
    int n;
    t_len *len;
    t_type *type;
    t_type t;
    t_port *port;
    t_ip *ip;
    char *buf = (char *)NULL;
    
    //Leemos primero el tipo en busca de un posible error
    n = recv(socket, &t, sizeof(t_type), MSG_PEEK);
    
    if (n <= 0)
	return -1;

    t = ntohs(t);

    if (t != DC)
    {
	recv_ack_err(socket);
	return -1;
    }
    
    //No hay error, leemos el paquete entero
    buf = (char *)malloc(DC_HEADER_SIZE);
    
    n = recv(socket, buf, DC_HEADER_SIZE, MSG_WAITALL);
    
    if (n <= 0)
    {
	free(buf);
	return -1;
    }
    
    type = (t_type *)buf;
    len = (t_len *)(buf+2);
    ip = (t_ip *)(buf+6);
    port = (t_port *)(buf+10);
    
    dc->header.type = ntohs(*type);
    dc->header.len = ntohl(*len);
    dc->ip.s_addr = *ip;
    dc->port = ntohs(*port);
    
    dc->version = (char *)malloc(dc->header.len);
    if(dc->version == NULL)
    {
	free(buf);
	return -1;
    }

    n = recv(socket, dc->version, dc->header.len, 0);
    
    if (n <= 0)
    {
	free(buf);
	return -1;
    }

    return 0;
}

