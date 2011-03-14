#include "mystringlib.h"

#define HOSTNAME_SIZE 32

#define ERR_PKG "WRONG PACKAGE"
#define ERR_PKG_LEN sizeof(ERR_PKG)

#define ERR_FILE "UNKNOWN FILE"
#define ERR_FILE_LEN sizeof(ERR_FILE)

#define OK_FILE "TENGO TU FICHERO PERO NO TE LO MANDO!!"
#define OK_FILE_LEN sizeof(OK_FILE)

#define ERR_TRANS "TRANSFER ERROR"
#define ERR_TRANS_LEN sizeof(ERR_TRANS)


typedef enum mode_e { TEXT, BIN } trans_mode;

void error(char *);
void do_connect(char *);
int init_client(void);
void set_folders(void);
void initialize(void);
void set_ident(char *);
int get_shared_files(filestruct *);
void get_list(char *);
void print_info(void);
void do_logout(void);
void do_whois(char *);
int send_file(int, int);
void set_mode (char *);
char *get_mode(void);
void *do_service(void *);
void *do_stuff(void *);
void terminate_client(void);
void get_file(int);
int recv_file(char *, int, int);

