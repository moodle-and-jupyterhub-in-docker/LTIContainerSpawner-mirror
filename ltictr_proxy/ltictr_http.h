
#include "https_tool.h"


int    send_http_response(int sock, SSL* ssl, int num, Buffer* buf);
int    send_http_error(int sock, SSL* ssl, int err, Buffer* opt);

char*  get_username_client(tList* hdr);

Buffer get_user_proxyinfo(char* api_host, int api_port, SSL_CTX* ctx, char* uname, char* token);

//int  recv_https_request(int sock, SSL* ssl, tList** lst, Buffer* buf);
//int  get_tcp_socket(int* port);
