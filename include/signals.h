#if !defined (_SIGNALS_H_)
#define _SIGNALS_H_

void SIG_INT_HANDLER (int);
void SIG_QUIT_HANDLER (int);
void SIG_PIPE_HANDLER (int);
void SIG_TERM_HANDLER (int);
void set_signals (void);
void terminate (void);

#endif

