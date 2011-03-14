#if !defined (_SCF_SERVER_H_)
#define _SCF_SERVER_H_

#include <arpa/inet.h>

#define HOSTNAME_SIZE 32

#define WELLCOME_MSG " \
############################################################# \
##    Wellcome to the Julian & Francisco SCF Server        ## \
############################################################# \
##  After 300 secs of inactivity you will be disconnected  ## \
#############################################################\n"

#define WELLCOME_MSG_LEN sizeof (WELLCOME_MSG)

#define ERR_PROT "PROTOCOL 0.2"
#define ERR_PROT_LEN sizeof (ERR_PROT)

#define ERR_ID "INVALID IDENT"
#define ERR_ID_LEN sizeof (ERR_ID)

#define USER_NOTFOUND "USER NOT FOUND"
#define USER_NOTFOUND_LEN sizeof (USER_NOTFOUND)

#define ERR_PKG "UNKNOWN OR UNEXPECTED MESSAGE"
#define ERR_PKG_LEN sizeof (ERR_PKG)

#define LOGOUT_MSG "Thanks for using the Julian & Francisco SCF Server. Goodbye!"
#define LOGOUT_MSG_LEN sizeof (LOGOUT_MSG)

typedef struct t_arg {
    int socket;
    struct in_addr ip;
} cli_args;    

void error(char *);
void init_server(void);
void print_server_info(void);
void *do_service(void *);
void *do_stuff(void *);
void shutdown_server(char *);

#endif

