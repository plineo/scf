/*
    SCF Client 
    Servidor de ficheros con protocolo SCF v.02 MULTIHILO
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include "signals.h"
#include "scf_client.h"
#include "scf.h"
#include "shell.h"

/* Socket para comunicarse con el servidor */
int srv_socket = 0;

/* Socket de escucha para otros clientes */
int cli_socket = 0;

struct hostent *server, *local_host;

struct sockaddr_in serv_addr, cli_addr;

//Puerto por donde escuchara el cli_socket
int my_port;

char my_host[HOSTNAME_SIZE+1];

//identificador o nick para identificarse en el servidor
char *my_ident;

//descriptores para hilos
pthread_t main_thread, server_thread; 

//modo de transferencia, por defecto fichero ordinario
trans_mode mode = TEXT;

//Para almacenar los ficheros que nos envie el servidor
filestruct list;

//directorio donde se almacenan los ficheros que se descargan
char *folderin = (char *)NULL;

//directorio donde se encuentran los ficheros que se comparten
char *folderout = (char *)NULL;


/****************************************************************************/
/*			         MAIN					    */	
/****************************************************************************/

int main(int argc, char *argv[])
{
    set_folders();
    initialize();
    set_signals();
    
    //Iniciar el cliente como servidor de ficheros
    if (init_client() == -1)
	error("Error on init client");

    while(1)
     {
          userin();
          procline();
     }
}

/***************************************************************************/

void error(char *msg)

{
    perror(msg);
    exit(0);
}

/* 
 Conectar con servidor SCF
 hostname: nombre del servidor SCF, tmabien puede ser una direccion IP
*/

void do_connect(char *hostname)
{
    int ack;
    filestruct files;
     
    if (srv_socket > 0)
    {
	printf("Already connected to server. Logout first.\n");
	return;
    }

    if (hostname == NULL)
    {
        printf("Enter a valid hostname\n");
        return;
    }

    server = gethostbyname(hostname);

    if (server == NULL)
     {
        //printf("%s: No such host!\n", hostname);
	perror(hostname);
        return;
     }
            
    srv_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (srv_socket < 0)
    {
        perror("ERROR opening socket");
        return;
    }

     bzero((char *) &serv_addr, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
     serv_addr.sin_port = htons(PORT);

     printf("Trying %s (%s).....\n", hostname, inet_ntoa(serv_addr.sin_addr));

     //Conectar con el servidor
     if (connect(srv_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
     {
        perror(hostname);
	srv_socket = close(srv_socket);
        return;
     }

    //Iniciar protocolo de conexion con servidor SCF
    if (send_ec_pkg(srv_socket, my_ident, my_port) == -1)
    {
	srv_socket = close(srv_socket);
        return;
    }

    ack = recv_ack_err(srv_socket);
    
    /* Error en el canal */
    if (ack == -1)
    {
	srv_socket = close(srv_socket);
        return;
    }
    
    /* Si el servidor rechaza nuestro paquete EC, optamos por abandonar */
    if (ack == 1)
    {
	send_fc_pkg(srv_socket);
	if (recv_ack_err(srv_socket) == 0)
	    srv_socket = close(srv_socket);
        return;
    }
	
    //El servidor acepta nuestro EC, ahora le enviamos la lista de ficheros
    get_shared_files(&files);
    send_default_pkg(srv_socket, files.lfiles, files.files, LF);
    ack = recv_ack_err(srv_socket);
    
    if (ack == 0) 
	printf("Connected to SCF Server %s\n", hostname);
    else
    {
	send_fc_pkg(srv_socket);
	if (recv_ack_err(srv_socket) == 0)
	    srv_socket = close(srv_socket);
    }
}

/* Inicializar el cliente como servidor de de los ficheros */
int init_client(void)
{
    socklen_t addr_len = sizeof(cli_addr);
    
    /* Apertura del Socket de escucha */
    cli_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (cli_socket < 0)
    {
        printf("ERROR opening client socket\n");
        return -1;
    }
    
    /* ligar el socket con el puerto */
    if (bind(cli_socket, (struct sockaddr *) &cli_addr,sizeof(cli_addr)) < 0)
    {
          error("ERROR on binding client socket\n");
          return -1;
    }

    listen(cli_socket, 5);

    if (getsockname(cli_socket, (struct sockaddr *)&cli_addr, &addr_len) != 0)
    {
	error("Error on getsockname");
	return -1;
    }

    my_port = ntohs(cli_addr.sin_port);
    
    if (pthread_create(&main_thread, NULL, do_service, (void *)NULL) != 0)
       error("PANIC!! creating thread!!");    
    
    return 0;
}

/*
 Rutina para inicializar los directorios de entrada y salida
*/

void set_folders()
{
    DIR *dir;

    folderin = getenv("SCFIN");
    if (folderin == NULL)
    {
	printf("Environment SCFIN not found\n");
	exit(0);
	return;
    }

    folderout = getenv("SCFOUT");
    if (folderout == NULL)
    {
	printf("Environment SCFOUT not found\n");
	exit(0);
	return;
    }

    
    if ((dir = opendir (folderin)) == NULL)
    {
	error(folderin);
	return;
    }
    
    closedir (dir);
    
    if ((dir = opendir (folderout)) == NULL)
    {
	error(folderout);
	return;
    }
    
    closedir (dir);
}

/* Obtiene los ficheros que este cliente va a compartir */
int get_shared_files(filestruct *f)
{
    DIR *dir;
    struct dirent *dir_entry;
    struct stat file_stat;
    char fichero[256];
    char *aux;
    char s[10];
    int len = 0, i = 0;

    f->lfiles = 0;
    f->nfiles = 0;

    /* Si no existe el directorio, no podemos continuar */    
    if ((dir = opendir (folderout)) == NULL)
	return -1;
        
    /* Recorre todo el directorio para calcular espacio necesario */
    while ((dir_entry = readdir (dir)) != NULL)
    {
	//hacer comprobacion de si es un fichero ordinario
	sprintf(fichero, "%s/%s", folderout, dir_entry->d_name);
	
	if (stat(fichero, &file_stat) == -1)
	    continue;
	
	if ((file_stat.st_mode & S_IFMT) != S_IFDIR)
	{
	    //calcular el tamagno total, incluido el \0
	    len += strlen(dir_entry->d_name) + 1;
	    //meter ademas el tamano del fichero
	    sprintf(s,"%d", (int)file_stat.st_size);
	    len += strlen(s) + 1;
	    i++;    
	}

    }

    // si no se comparten ficheros, se lo advertimos al usuario    
    if (i == 0)
    {
	closedir(dir);
	printf("\nWARNING: You are not sharing any files!!\n");
	return 1;
    }
    
    f->files = (char *)malloc(len*sizeof(char));
    if (f->files == NULL)
    {
	perror("Reading files");
	return -1;
    }
    aux = f->files;
    rewinddir(dir);
    
    /* Recorre todo el directorio para almacenamiento*/
    while ((dir_entry = readdir (dir)) != NULL)
    {
	//hacer comprobacion de si es un fichero ordinario
	sprintf(fichero, "%s/%s", folderout, dir_entry->d_name);
	
	if (stat(fichero, &file_stat) == -1)
	    continue;
	
	if ((file_stat.st_mode & S_IFMT) != S_IFDIR)
	{
	    strcpy(f->files, dir_entry->d_name);
	    f->files += strlen(dir_entry->d_name) + 1;
	    // aqui convertir entero a cadena
	    sprintf(s,"%d", (int)file_stat.st_size);
	    strcpy(f->files, s);
	    f->files += strlen(s) + 1;
	}
    }
    
    f->files = aux;
    f->lfiles = len;
    f->nfiles = i;
    closedir (dir);
    
    return 0;
}


/* Inicializacion del cliente */
void initialize()
{
    gethostname(my_host,HOSTNAME_SIZE);
    my_host[HOSTNAME_SIZE] = '\0';
    
    if ((local_host = gethostbyname(my_host)) == NULL)
    {
	printf("Local server not found.\n");
    	return;
    }
    
    list.nfiles = 0;
    list.lfiles = 0;
    list.files = (char *)NULL;
    
    
    bzero((char *) &cli_addr, sizeof(cli_addr));

    cli_addr.sin_family = AF_INET;
    bcopy((char *)local_host->h_addr_list[0],(char *)&cli_addr.sin_addr.s_addr,local_host->h_length);
    cli_addr.sin_port = htons(0);

    my_ident = (char *)savestring(getenv("LOGNAME"));

}

/* Rutina para cambiar o visualizar el identificador */
void set_ident(char *ident)
{
    if (srv_socket > 0)
    {
	printf("You can't change your nick connected to server. Logout first.\n");
	return;
    }

    if (ident == NULL)
	printf("Your nick is %s.\n", my_ident);
    else
    {
	free(my_ident);
	my_ident = (char *)savestring(ident);
	printf("Your nick is now %s.\n", my_ident);
    }
}

/* Mostrar algo de informacion */

void print_info()
{
    printf("%s (%s) SCF Client\nversion: %s\nport: %d\n", my_host, inet_ntoa(cli_addr.sin_addr), VERSION, my_port);
    printf("Using %s file transfer mode.\n", get_mode());    
}


/* 
 Rutina para pedir al servidor una lista de ficheros que concuerde con
 el patron dado en el argumento pattern
*/

void get_list(char *pattern)
{
    int len;
    default_pkg lf;
    
    if (srv_socket <= 0)
    {
	printf("You are not connected to a SCF server.\n");
	return;
    }
    
    len = strlen(pattern) + 1;

    // Enviar la consulta al servidor
    if (send_default_pkg(srv_socket, len, pattern, CON) == -1)
    {
	srv_socket = close(srv_socket);
	return;
    }

    if (recv_default_pkg(srv_socket, &lf) == -1)
    {
	srv_socket = close(srv_socket);
	return;
    }
    else
    {
	//Comprobar su respuesta
	if (lf.type != LF)
	{
	    send_default_pkg(srv_socket, ERR_PKG_LEN, ERR_PKG, ERR);
	    return;
	}
	else
	    send_default_pkg(srv_socket, 0, NULL, ACK);
	
	//Nos envia una lista vacia, ninguna coincidencia    
	if (lf.len <= 0)
	{
	    printf("No matches found.\n");
	    list.nfiles = 0;
	    list.lfiles = 0;
	    return;
	}
	
	// Nos envia algo, vamos a mostrarlo por pantalla y 
	// almacenar los datos para que luego se puedan pedir con 'get'
	
	list.nfiles = 0;
	list.lfiles = lf.len;
	list.files = (char *)lf.data;
	printf("Id  Filename\033[45GSize (bytes)\033[60GUser\n");
	while (*lf.data != '\0')
	{
	    list.nfiles++;
	    /* imprime el Id del fichero y su nombre */	    
	    printf("[%d] \033[1;33m%s", list.nfiles, lf.data);
	    lf.data += strlen(lf.data) + 1;
	    
	    /* imprime el tamaño */
	    printf("\033[45G%s", lf.data);
	    lf.data += strlen(lf.data) + 1;
	    
	    /* imprime el propietario */
	    printf("\033[60G\033[1;31m%s\n\033[0;39m", lf.data);
	    lf.data += strlen(lf.data) + 1;
	}
	printf("\n");
    }
}

/* Solicitar desconexion del servidor */
void do_logout(void)
{
    if (srv_socket <= 0)
    {
	printf("You are not connected to a SCF server.\n");
	return;
    }
    
    if (send_fc_pkg(srv_socket) == -1)
    {
	srv_socket = close(srv_socket);
	return;
    }
    else
    {
	if (recv_ack_err(srv_socket) == 0)
	{
	    srv_socket = close(srv_socket);
	    printf("Logout successful.\n");
	}
	else
	    printf("Logout failed!\n");
    }
}

/* Rutina para pedir informacion sobre un usuario */
void do_whois(char *ident)
{
    dc_pkg dc;
    int len;
    
    if (srv_socket <= 0)
    {
	printf("You are not connected to a SCF server.\n");
	return;
    }
    
    len = strlen(ident) + 1;
    
    if (send_default_pkg(srv_socket, len, ident, PDC) == -1)
    {
    	srv_socket = close(srv_socket);
	return;
    }

    if (recv_dc_pkg(srv_socket, &dc) == 0)
    {
        printf("\nident: \t%s\n", ident); 
	printf("IP: \t%s\n", inet_ntoa(dc.ip)); 
	printf("port: \t%d\n", dc.port); 
	printf("ver: \t%s\n\n", dc.version); 
    }

}

/* Rutina de inicializacion del hilo que acepta conexiones de otros clientes */
void *do_service(void *arg)
{
    int nsfd;
	socklen_t clilen;
    struct sockaddr_in cli;
    
    clilen = sizeof(cli);    

    while(1)
    {    
        nsfd = accept(cli_socket,(struct sockaddr *) &cli, &clilen);
	
        if (nsfd < 0)
           error("ERROR on accept");

	if (pthread_create(&server_thread, NULL, do_stuff, (void *)nsfd) != 0)
           error("Error creating thread!!");
    }
}

/* Rutina de inicializacion del hilo servidor de peticiones a los clientes */

void *do_stuff(void *arg)
{	
    int sock = (int)arg;
    int fd;
    default_pkg pf;
    char *file;

    
    bzero(&pf, sizeof(default_pkg));
    //esperar la llegada de un paquete
    recv_default_pkg(sock, &pf);
        
    //si el paquete no es PF, cerramos
    if ((pf.type != PF) || (pf.len <= 1))
    {
	send_default_pkg(sock, ERR_PKG_LEN, ERR_PKG, ERR);
	close(sock);
	pthread_exit(0);
	return((void *)0);
    }

    file = (char *)malloc(pf.len+strlen(folderout)+1);
    /* Hay un error al reservar memoria, posiblemente la longitud es incorrecta */
    if (file == NULL)
    {
    	send_default_pkg(sock, ERR_PKG_LEN, ERR_PKG, ERR);
	close(sock);
	pthread_exit(0);
	return((void *)0);
    }

    sprintf(file, "%s/%s", folderout, pf.data);
    
    //Intentamos abrir el fichero /$HOME/scf/salida/+ fichero
    fd = open(file, O_RDONLY);
    
    if (fd == -1)
    {
	send_default_pkg(sock, ERR_FILE_LEN, ERR_FILE, ERR);
	close(sock);
	pthread_exit(0);
	return((void *)0);
    }

    //si llegamos aqui, es que el cliente nos pide un fichero que tenemos
    //asi que le intentamos mandar el fichero que nos pide

    if (send_file(fd, sock) == -1)
	send_default_pkg(sock, ERR_TRANS_LEN, ERR_TRANS, ERR);
	
    close(fd);
    close(sock);
    pthread_exit(0);
    return ((void *)0);
}

/*
    Rutina para establcer el modo en el que los ficheros 
    se interpretan:
	TEXT: el fichero es ordinario
	BIN: el fichero es ejecutable
*/

void set_mode(char *m)
{
    if (m == NULL)
	printf("file transfer mode is %s\n", get_mode());
    else
    {
	if (strcmp(m, "text") == 0)
	{
	    mode = TEXT;
	    printf("file transfer mode is now TEXT.\n");
	}
	else
	if (strcmp(m, "bin") == 0)
	{
	    mode = BIN;
	    printf("file transfer mode is now BIN.\n");
	}
	else
	    printf("Unknown mode!\n");
    }
}

char *get_mode()
{
    switch(mode)
    {
	case TEXT:
	    return("TEXT");
	    break;
	case BIN:
	    return("BIN");
	    break;
    }
    return("TEXT");
}

/*
    Rutina para enviar un fichero
    
    fd: descriptor del fichero a enviar
    socket: canal por donde se envia
    
    Ambos descriptores deben estar abiertos antes de la llamada.
    Retorna 0 si todo ha ido bien, -1 si se produjo un error en el envio.
    No cierra ningun descriptor.
*/

int send_file (int fd, int socket)
{
    char *buf;
    int bread = 0, btotal = 0;
    struct stat file_info;
    size_t size;
    
    buf = (char *)malloc(BLOCK_SIZE);
    
    // obtener tamano del fichero
    fstat(fd, &file_info);
    size = file_info.st_size;

    while (btotal < size)
    {
	bread = read(fd, buf, BLOCK_SIZE);
	if (bread == -1)
	{
	    free(buf);
	    return -1;
	}

	if (send_default_pkg (socket, bread, buf, DF) == -1)
	{
	    free(buf);
	    return -1;	
	}
	btotal += bread;
    }
    
    free(buf);
    return 0;
}

void terminate_client(void)
{
    if (srv_socket > 0)
	do_logout();
    cli_socket = close(cli_socket);
    exit(0);
}

/*
 Rutina para solicitar la descarga de un fichero 
 n: identificador del fichero a descargar
 
 Esta rutina se ejecuta tras una llamada al comando 'get'
 Antes de usar este comando, debemos especificar una consulta mediante 'list'
 
*/

void get_file(int n)
{
    dc_pkg dc;
    int aux, len;
    char *ident, *file;
    struct sockaddr_in c;
    struct timeval timeout = {5,0}; /* 5 sec */
    struct timeval tini, tend, tdif;
    struct timezone tz;
    float speed;
    int timeout_len = sizeof(timeout);
    int sock;
    int size = 0;


    if (list.nfiles == 0)
    {
	printf("\nfile list is empty. Use list command to get a list of files.\n");
	return;
    }
    
    if ((n > list.nfiles) || (n <= 0))
    {
	printf("\nUnknown file!\n");
	return;
    }

    // Buscamos el fichero y el propietario 
    // en la lista que hemos obtenido con 'list'    
    aux = n - 2;
    aux += 2*n - 1;

    file = list.files;
            
    while(aux > 0)
    {
	if (*file == '\0')
	    aux--;
	file++;    
    }

    ident = file + strlen(file) + 1;
    size = atoi(ident);
    ident += strlen(ident) + 1;
        
    len = strlen(ident) + 1;
    
    printf("\nReceiving information about %s: ", ident);
    
    // Solicitamos al servidor informacion sobre el propietario del fichero
    if (send_default_pkg(srv_socket, len, ident, PDC) == -1)
    {
	srv_socket = close(srv_socket);
	return;
    }

    if (recv_dc_pkg(srv_socket, &dc) == 0)
	printf("\033[60G\033[1;32mOK\033[0;39m\n");
    else
    {
    	printf("\033[60G\033[1;31mFAILED\033[0;39m\n");
	return;
    }
    
    //Comprobacion de la version del protocolo que usa el propietario
    if (strcmp(dc.version, VERSION) != 0)
    {
	printf("Incompatible SCF Protocol.\n");
	return;
    }
    
    printf("Connecting to %s: ", ident);
    
    //Abrir un socket para comunicarnos con el propietario
    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sock < 0)
    {
	printf("\033[60G\033[1;31mFAILED\033[0;39m\n");
        perror("ERROR opening socket");
	return;
    }

    bzero((char *) &c, sizeof(c));

    c.sin_family = AF_INET;
    c.sin_addr = dc.ip;
    c.sin_port = htons(dc.port);
    
    // Conectar con el cliente
    if (connect(sock,(struct sockaddr *)&c,sizeof(c)) < 0)
    {
	printf("\033[60G\033[1;31mFAILED\033[0;39m\n");
        perror("Error connecting");
	close(sock);
        return;
    }
    
    //tenemos que establecer un TIME_OUT para evitar que nos deje colgados
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, timeout_len) != 0)
    	printf("WARNING!! No Time-Out set!");
    
    
    printf("\033[60G\033[1;32mOK\033[0;39m\n");
    printf("Receiving file %s (%d bytes): ", file, size);
    fflush(stdout);

    /* Recibir fichero */
    len = strlen(file);
    send_default_pkg(sock, len, file, PF);
    
    gettimeofday(&tini, &tz);
    aux = recv_file(file, size, sock);
    gettimeofday(&tend, &tz);

    if (tini.tv_usec > tend.tv_usec)
    {
	tend.tv_usec += 1000000;
	--tend.tv_sec;
    }

    tdif.tv_sec = tend.tv_sec - tini.tv_sec;
    tdif.tv_usec = tend.tv_usec - tini.tv_usec;
    speed = tdif.tv_sec + (tdif.tv_usec * 1.0e-6);
    speed = (size / speed) / 1024;

    if (aux > 0)
    	printf("\n%d Packets received\nTime: %ld.%06ld s\nAverage speed: %.1f KB/s\n\n", 
	    aux, (long int)tdif.tv_sec, (long int)tdif.tv_usec, speed);
    else
	printf("\033[60G\033[1;31mFAILED\033[0;39m\n");
	
    /* Cerrar el socket */
    close(sock);
    return;
}


/*
    Rutina para recibir un fichero
    
    file: nombre del fichero que queremos recibir
    size: tamano del fichero
    socket: canal por donde se recibe
    
    el canal debe estar abierto antes de la llamada.
    Retorna 0 si todo ha ido bien, -1 si se produjo un error en la recepcion.
    No cierra el canal.
*/

int recv_file (char *file, int size, int socket)
{
    char *path;
    int fd, i = 0;
    float percent = 0.0, r_size = 0.0;
    default_pkg df;
    mode_t m;
    
    //Esperamos que nos envie algo
    if (recv_default_pkg(socket, &df) == -1)
    	return -1;
    
    if (df.type == ERR)
    {
	if (df.len > 0)
	    printf("%s\n", df.data);
	else
	    printf("Unknown error on transfer file\n");
	    
	return -1;
    }
    
    r_size = df.len;
    
    path = (char *)malloc(strlen(file)+strlen(folderin)+1);
    if (path == NULL)
    {
    	printf("Error creating file %s\n", path);
	return -1;
    }
    
    sprintf(path, "%s/%s", folderin, file);
    
    //Establecer los permisos del fichero
    if (mode == BIN)
	m = S_IRWXU;
    else
	m = S_IRUSR + S_IWUSR;

    fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, m);
    if (fd == -1)
    {
	printf("Error creating file %s\n", path);
	return -1;
    }
    
    if (write(fd, df.data, df.len) == -1)
    {
	close(fd);
	return -1;
    }
    
    percent = (r_size*100) / size;
    printf("\033[60G\033[1;37m%.1f%%\033[0;39m", percent);

    while (r_size < size)
    {
	
	if (recv_default_pkg(socket, &df) == -1)
	{
	    close(fd);
	    return -1;
	}
	
	
	if (write(fd, df.data, df.len) == -1)
	{
	    close(fd);
	    return -1;
	}
	
	r_size += df.len;
	percent = (r_size*100) / size;
	printf("\033[60G\033[1;37m%.1f%%\033[0;39m", percent);
	fflush(stdout);
	i++;
    }
    
    close(fd);
    return (i+1);
}

/****************************************************************************/
/*				THE END 				    */	
/****************************************************************************/
