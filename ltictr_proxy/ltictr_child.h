
#include "ltictr_xmlrpc.h"
#include "ltictr_https.h"
#include "ipaddr_tool.h"


void   receipt_child(int ssock, SSL_CTX* c_ctx, SSL_CTX* s_ctx, tList* lp);

char*  get_sessionid_from_header(tList* hdr);
char*  get_info_from_cookie(tList* hdr);
char*  get_info_from_sessioninfo();

int    init_process(int dummy, char* client);
int    term_process(int dummy);


int    send_client(int sock, SSL* ssl, tList* hdr, Buffer buf, int http_com);
int    send_server(int sock, SSL* ssl, tList* hdr, Buffer buf, int http_com, char* proto);

int    get_proxy_socket(tList* hdr, int* http_com, tList* lproxy);
SSL*   get_proxy_ssl(int sock, tList* hdr, SSL_CTX* ctx);
