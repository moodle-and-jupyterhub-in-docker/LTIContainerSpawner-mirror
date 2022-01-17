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

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>




///////////////////////////////////////////////////////////////////////////////////

int   main(int argc, char** argv);
void  receipt(char* hostname, int cport, struct sockaddr addr, SSL_CTX* s_ctx, SSL_CTX* c_ctx);
void  sig_term(int signal);
void  sig_child(int signal);

int   server_api(int sock);
int   send_http_error(int sock, int err);
int   get_notused_tcp_port(int port);

///////////////////////////////////////////////////////////////////////////////////

int  fe_server(int csofd, int nsofd, SSL* cssl, SSL* sssl, char* msg, int cc);  // S -> C   fesrv is C
int  fe_client(int nsofd, int csofd, SSL* sssl, SSL* cssl, char* msg, int cc);  // C -> S   fesrv is S
int  init_main(int mode, tList* file);
int  term_main(void);
int  init_process(int, char*);
int  term_process(int);



#endif

