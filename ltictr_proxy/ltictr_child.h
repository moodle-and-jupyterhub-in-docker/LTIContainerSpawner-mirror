
#include "ltictr_xmlrpc.h"
#include "ltictr_http.h"
#include "ipaddr_tool.h"
#include "https_tool.h"


void   receipt_child(int ssock, SSL_CTX* s_ctx, tList* lp);

char*  get_sessionid_from_header(tList* hdr);
char*  get_info_from_cookie(tList* hdr);
char*  get_info_from_sessioninfo(char* mesg);

int    init_process(int dummy, char* client);
int    term_process(int dummy);

int    send_client(int sock, SSL* ssl, tList* hdr, Buffer buf);
int    send_server(int sock, SSL* ssl, tList* hdr, Buffer buf, char* proto);

int    get_proxy_socket(tList* hdr);
SSL*   get_proxy_ssl(int sock, SSL_CTX* ctx, tList* hdr);

char*  get_proxy_username(tList* hdr);
Buffer get_proxy_target(char* api_host, int api_port, SSL_CTX* ctx, char* uname, char* token);

