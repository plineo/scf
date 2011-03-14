#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "signals.h"

#ifndef SIG_IGN
#define SIG_IGN ((void (*) ())1)
#endif
#ifndef SIG_ERR
#define SIG_ERR ((void (*) ())-1)
#endif

/* Manejadores de las signals */
void SIG_INT_HANDLER (int sig)
{
    if (signal (SIGINT, SIG_IGN) == SIG_ERR)
    {
	perror ("Noooooooo!");
	exit (-1);
    }
}

void SIG_PIPE_HANDLER (int sig)
{
    if (signal (SIGPIPE, SIG_IGN) == SIG_ERR)
    {
	perror ("Noooooooo!");
	exit (-1);
    }
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
    printf("SCF Server is down.\n");
    kill(0, SIGTERM);
}

/* Inicializacion de signals */
void set_signals (void)
{
    //las ignoramos
    signal (SIGINT, SIG_INT_HANDLER) ;
    signal (SIGQUIT, SIG_QUIT_HANDLER);
    signal (SIGPIPE, SIG_PIPE_HANDLER);
}


