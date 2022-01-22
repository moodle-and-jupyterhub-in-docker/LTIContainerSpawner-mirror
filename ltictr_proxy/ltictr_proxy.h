/* vi: set tabstop=4 nocindent noautoindent: */

#ifndef _LTICTRPROXY_H
#define _LTICTRPROXY_H


#include "tools.h"
#include "buffer.h"
#include "tlist.h"
#include "network.h"
#include "ssl_tool.h"
#include "ipaddr_tool.h"
#include "protocol.h"
#include "ltictr_signal.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
//#include <signal.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>




///////////////////////////////////////////////////////////////////////////////////

int   main(int argc, char** argv);
int   init_main(Buffer file);
void  term_main(int code);
void  close_all_socket(tList* lp);

//void  sig_term(int signal);
//void  sig_child(int signal);



#endif

