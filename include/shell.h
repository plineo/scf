#if !defined (_SHELL_H_)
#define _SHELL_H_

#define EOL 1
#define ARG 2
#define DDOT 3

#define MAXARG 32
#define MAXBUF 32
#define MAX_SERVER_COMMANDS 6
#define MAX_CLIENT_COMMANDS 10

void userin (void);
int gettok (char **);
void procline (void);
int run_command (char **);
void print_help(void);

#endif
