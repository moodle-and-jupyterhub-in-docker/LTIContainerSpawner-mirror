
#include "https_tool.h"


int   send_https_response(int sock, SSL* ssl, int num, Buffer* buf);
int   send_https_error(int sock, SSL* ssl, int err, Buffer* opt);

char* get_username_http(tList* hdr);

//int  recv_https_request(int sock, SSL* ssl, tList** lst, Buffer* buf);
//int  get_tcp_socket(int* port);