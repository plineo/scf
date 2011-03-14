/* 
 
 SERVIDOR SCF MULTIHILO SIN LIMITE DE CONEXIONES 

 Prioridad para los hilos que quieran consultar (lectores) frente
 a escritores 

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>

#include "scf_server.h"
#include "scf.h"
#include "shell.h"
#include "scfdb.h"
#include "signals.h"

// Socket principal
static int sfd;

// Identificadores de hilo

static pthread_t main_thread; // hilo que acepta conexiones
static pthread_t server_thread; // hilo que sirve peticiones

//Semaforo para acceder a la varible readcount, para los hilos lectores
sem_t mutex;

//Semaforo para los hilos que quieran escribir
sem_t wrt;

//Numero de hilos que se encuentra leyendo
int readcount = 0;

static char whoami[HOSTNAME_SIZE+1];

static struct hostent *local_host;

static struct sockaddr_in serv_addr;


/************************************************************************/
/*			    MAIN					*/
/************************************************************************/


int main (int argc, char *argv[])
{
    //Inicializacion del servidor
    init_server();
    print_server_info();

    //Inicializacion de los semaforos
    sem_init(&mutex, 0, 1);
    sem_init(&wrt, 0, 1);
    
    //se crea un hilo para atender peticiones
    if (pthread_create(&main_thread, NULL, do_service, (void*)NULL) != 0)
       error("PANIC!! creating thread!!");      

    //Aqui continua el servidor interactivo
    //podemos introducir comandos
    while(1)
    {
	    userin();
	    procline();
    }
}


void error(char *error_msg)
{
    perror(error_msg);
    exit(1);
}


/* Inicializa el servidor */

void init_server(void)
{
    /* Iniciar los manejadores de signals para evitar caidas */
    set_signals();

    /* Iniciar la base de datos */
    init_scfdb();

    /* Obtiene informacion sobre el servidor local */
    gethostname(whoami,HOSTNAME_SIZE);
    whoami[HOSTNAME_SIZE] = '\0';

    if ((local_host = gethostbyname(whoami)) == NULL)
	error("Local server not found!!");

    /* Apertura del Socket de escucha */
    sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sfd < 0)
        error("ERROR opening socket!!");
    
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    bcopy((char *)local_host->h_addr_list[0],(char *)&serv_addr.sin_addr.s_addr,local_host->h_length);
    serv_addr.sin_port = htons(PORT);

    /* ligar el socket con el puerto */
    if (bind(sfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
    {
	close(sfd);
	error("ERROR on binding");
    }
	   
    /* Se establece una cola de 5 clientes */
    listen(sfd, 5);
    
    return;
}

/* Muestra informacion sobre el servidor, por pantalla */ 
void print_server_info(void)
{
    printf("SCF Server running on %s (%s)\n",whoami,inet_ntoa(serv_addr.sin_addr));
    printf("Version: %s\n", VERSION);
    printf("Port: %d\n", PORT);
}

/* 

 
 Hilo main_thread
    
 Funcion de inicializacion de el hilo que se encarga de 
 aceptar peticiones de conexion por parte de los clientes
 
*/

void *do_service(void *arg)
{
    int nsfd;
    socklen_t clilen;
    struct timeval time_out = {300,0}; /* Tiempo maximo de inactividad, 300 s */
    int time_len = sizeof(time_out);
    struct sockaddr_in cli_addr;
    cli_args *args;
    
    clilen = sizeof(cli_addr);

    while(1)
    {
	/* esperar conexiones */
        nsfd = accept(sfd,(struct sockaddr *) &cli_addr, &clilen);
        if (nsfd < 0)
	{
    	    pthread_exit(0);
	    return ((void *)0);
	}
           
	/* 
	  Lo siguiente solo funciona en la version RH 7.2
	  la version 6.2 no permite hacer esto, desconocemos por que!!
	*/ 
	setsockopt(nsfd, SOL_SOCKET, SO_RCVTIMEO, &time_out, time_len);

	/* argumentos que pasaremos al hilo que atiende esta peticion */
	args = (cli_args *)malloc(sizeof(cli_args));           
	args->socket = nsfd;
	args->ip = cli_addr.sin_addr;
	
	/* crear el hilo para atender a este cliente */
	if (pthread_create(&server_thread, NULL, do_stuff, (void *)args) != 0)
	{
	    free(args);
	    close(nsfd);
    	    pthread_exit(0);
	    return ((void *)0);
	}

    }
}

/* La funcion mas importante, atender al cliente */
void *do_stuff(void *arg)
{
    cli_args *params = (cli_args *)arg;
    int sock;
    ec_pkg ec;
    default_pkg pkg;
    CLI_ENTRY *c;
    int n, exit;
    filestruct *buf;
    
    sock = params->socket;

    //Esperar a que el cliente envie un paquete EC valido
    do 
    {
	exit = 0;
	bzero((char *) &ec, sizeof(ec));	

	exit = recv_ec_pkg(sock, &ec);

	//La comuncacion se ha roto o se ha superado el TIMEOUT establecido
	if (exit == -1)
	{
	    free(arg);
	    close(sock);
	    pthread_exit(0);
	    return ((void *)0);
	}

	/* Si se recibe un paque FC, el cliente */
	/* se arrepiente de conectar y cerramos */
	if (exit == 1)
	{		
	    send_default_pkg(sock, LOGOUT_MSG_LEN, LOGOUT_MSG, ACK);
	    
	    /*
	     Dorminos un poco al hilo antes de finalizar, ya que hemos encontrado
	     problemas al cerrar el socket, el cliente recibe un BROKEN PIPE
	     antes de haber leido en su totalidad el paquete ACK que le envia
	     el servidor
	    */
	    
	    sleep(1);
	    
	    free(arg);
	    close(sock);
	    pthread_exit(0);
	    return ((void *)0);
	}
	
	/* Si el paquete no es EC, algo va mal */	
	if (exit == 2)
	{
	    send_default_pkg(sock, ERR_PKG_LEN, ERR_PKG, ERR);
	    continue;
	}

	//Comprobar que el ident es valido
	if ((ec.ident == '\0') || (ec.header.len <= 0))
	{
    	    send_default_pkg(sock, ERR_ID_LEN, ERR_ID, ERR);
	    exit = 3;
    	    continue;
	}
	
	//Comprobamos la version del protocolo		
	if (strcmp(ec.ver, VERSION) < 0)
	{
	    send_default_pkg(sock, ERR_PROT_LEN, ERR_PROT, ERR);
	    exit = 4;
    	    continue;
    	}
	
	/*
	 Comprobar que no existe un ident con el mismo nombre
	 y lo hacemos en exclusion mutua para garantizar que no se
	 cuela ningun hilo que pudiera dar de alta un usuario con
	 el mismo IDENT
	*/

	sem_wait(&wrt);
	c = find_client(ec.ident);
	sem_post(&wrt);

	
	if (c != NULL)
	{
	    send_default_pkg(sock, ERR_ID_LEN, ERR_ID, ERR);
	    exit = 5;
	    continue;
	}
    } while (exit > 0);

    //Confirmamos que hemos aceptado su IDENT y su protocolo
    send_default_pkg(sock, 0, NULL, ACK);

    bzero((char *)&pkg, sizeof(pkg));	
    
    //Esperamos que el cliente nos envie la lista de ficheros
    if (recv_default_pkg(sock, &pkg) == -1)
    {
    	send_default_pkg(sock, ERR_PKG_LEN, ERR_PKG, ERR);
	free(arg);
	close(sock);
	pthread_exit(0);
	return ((void *)0);
    }

    //comprobamos que se trata de un paquete LF
    //No comprobamos la longitud, ya que puede que no comparta ningun
    //fichero
    if (pkg.type != LF)
    {
	send_default_pkg(sock, ERR_PKG_LEN, ERR_PKG, ERR);
	free(arg);
	close(sock);
	pthread_exit(0);
	return ((void *)0);
    }
    
    //Si llegamos hasta aqui, todo es correcto y el cliente
    //es dado de alta en el sistema
    
    sem_wait(&wrt);
    add_client(ec.ident, ec.ver, params->ip, ec.port, pkg.data, pthread_self());
    sem_post(&wrt);

    //Confirmamos que hemos aceptado su lista de ficheros dandole la bienvenida
    send_default_pkg(sock, WELLCOME_MSG_LEN, WELLCOME_MSG, ACK);
    
    
    /******************************************************************/
    /************* Analizar sus peticiones ****************************/
    /******************************************************************/
    
    while(1)
    {   
	//leer peticiones
	bzero((char *)&pkg, sizeof(pkg));	
	n = recv_default_pkg(sock, &pkg);

	//La comuncacion se ha roto o se ha sobrepasado el TIMEOUT
	if (n == -1)
	{
	    sem_wait(&wrt);
	    remove_client(ec.ident);
	    sem_post(&wrt);
	    free(arg);
	    close(sock);
	    pthread_exit(0);
	    return ((void *)0);
	}

	switch(pkg.type)
	{
	    case FC:
		//El cliente desea terminar la sesion,
		//lo damos de baja, y terminamos el hilo.
		n = send_default_pkg(sock, LOGOUT_MSG_LEN, LOGOUT_MSG, ACK);		
		sleep(1);
		sem_wait(&wrt);
		remove_client(ec.ident);
		sem_post(&wrt);
		free(arg);
		close(sock);
		pthread_exit(0);
		return ((void *)0);
		break;
		
	    case CON:
		//realizar busqueda de patrones
		
		//protocolo de entrada
		sem_wait(&mutex);
		readcount++;
		if (readcount == 1) 
			sem_wait(&wrt);
		sem_post(&mutex);
		
		//Acceso a la region critica
		buf = find_files(pkg.data, pthread_self());
		
		//Protocolo de salida
		sem_wait(&mutex);
		readcount--;
		if(readcount == 0) 
			sem_post(&wrt);
		sem_post(&mutex);
		
		//Enviar resultados de la consulta
		send_default_pkg(sock, buf->lfiles, buf->files, LF);
		recv_default_pkg(sock, &pkg);
		free(buf->files);
		free(buf);
		break;
		
	    case PDC:
		
		//buscar en la base de datos un usuario
		//Protocolo de entrada a la R.C.
		sem_wait(&mutex);
		readcount++;
		if (readcount == 1) 
			sem_wait(&wrt);
		sem_post(&mutex);
		
		// region critica
		c = find_client(pkg.data);
		
		// protocolo de salida
		sem_wait(&mutex);
		readcount--;
		if(readcount == 0) 
			sem_post(&wrt);
		sem_post(&mutex);

		//si existe enviar datos
		if (c != NULL)
		    send_dc_pkg(sock, c);
		else
		    send_default_pkg(sock, USER_NOTFOUND_LEN, USER_NOTFOUND, ERR);
				    
		break;
		
	    default:		    
		send_default_pkg(sock, ERR_PKG_LEN, ERR_PKG, ERR);
		break;
	}

    }
    
    /* Aqui nunca llegaremos, pero por si acaso */
    free(arg);
    close(sock);
    pthread_exit(0);
    return ((void *)0);
}

/* 
 Terminacion del servidor
 Modos de invocacion:
     shutdown now, termina inmediatamente el servidor
     shutdown wait, espera a que todos los hilos finalicen
 Por defecto es now.
*/
void shutdown_server(char *m)
{
    char s;
    
    if ((m == NULL) || (strcmp(m, "now")) == 0)
    {    
	printf("Are you sure you want to shutdown the server now? (y/n): ");
	scanf("%c", &s);

	if (s != 'y')
	    return;
	
	sem_wait(&wrt);
	clear_clients();
	close(sfd);
	sem_destroy(&mutex);
	sem_destroy(&wrt);	
	terminate();
    }
    
    if (strcmp(m, "wait") == 0)
    {
	printf("Waiting all connections to be closed.....");
	fflush(stdout);

	//Cerrar el canal para indicar a los clientes
	//que quieran conectarse que el servidor esta down.
	close(sfd);

	//cancelar al hilo que acepta peticiones para que no acepte mas
	pthread_cancel(main_thread);

	//Esperar a que todos los hilos hayan terminado
	while (clientcount() != 0);
	
	printf("OK\n");
	fflush(stdout);
	
	//liberar la memoria y terminar
	clear_clients();
	sem_destroy(&mutex);
	sem_destroy(&wrt);	
	
	terminate();
    }
    else
	printf("Unknown shutdown mode.\n");
}

/************************************************************************/
/*			    THE END					*/
/************************************************************************/

