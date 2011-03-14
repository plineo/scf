#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "signals.h"

#ifndef SIG_IGN
#define SIG_IGN ((void (*) ())1)
#endif
#ifndef SIG_ERR
#define SIG_ERR ((void (*) ())-1)
#endif

//Rotura del canal de comunicacion
void SIG_PIPE_HANDLER (int sig)
{
    if (signal (SIGPIPE, SIG_IGN) == SIG_ERR)
    {
	perror ("Nooooooooo!");
	exit (-1);
    }
    else
	printf("Conexion Perdida!!\n");
}

void SIG_QUIT_HANDLER (int sig)
{
    if (signal (SIGQUIT, SIG_IGN) == SIG_ERR)
    {
	perror ("Nooooooooo!");
	exit (-1);
    }
}

/* Terminacion */
void terminate (void)
{
    kill (0, SIGTERM);
}

/* Inicializacion de signals */
void set_signals (void)
{
    signal (SIGQUIT, SIG_QUIT_HANDLER);
    signal (SIGPIPE, SIG_PIPE_HANDLER);
}


