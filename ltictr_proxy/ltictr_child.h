
#include "ltictr_xmlrpc.h"
#include "ltictr_https.h"
#include "ipaddr_tool.h"


void   receipt_child(char* hostname, int cport, int ssock, SSL_CTX* c_ctx, SSL_CTX* s_ctx, tList* lp);

char*  get_sessionid_from_header(char* mesg);
char*  get_info_from_sessioninfo(char* mesg);
char*  get_info_from_cookie(char* mesg);

int    init_process(int dummy, char* client);
int    term_process(int dummy);

int    fe_server(int dummy1, int sofd, SSL* dummy2, SSL* ssl, char* mesg, int cc);
int    fe_client(int dummy1, int cofd, SSL* dummy2, SSL* ssl, char* mesg, int cc);
