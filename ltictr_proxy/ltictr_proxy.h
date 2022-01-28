
#include "ltictr_xmlrpc.h"
#include "ipaddr_tool.h"
#include "https_tool.h"


void   receipt_proxy(int ssock, SSL_CTX* s_ctx, SSL_CTX* c_ctx, Buffer api_host, tList* lp);

int    send_client(int sock, SSL* ssl, tList* hdr, Buffer buf);
int    send_server(int sock, SSL* ssl, tList* hdr, Buffer buf, char* proto);
//
int    get_proxy_socket(tList* lst);
SSL*   get_proxy_ssl(int sock, SSL_CTX* ctx, tList* lst);
char*  get_proxy_username(tList* hdr);
Buffer get_proxy_target(char* apiserver, int port, SSL_CTX* ctx, char* uname, char* token);

char*  get_sessionid_from_header(tList* hdr);
char*  get_info_from_cookie(tList* hdr);
char*  get_info_from_sessioninfo(char* mesg);

