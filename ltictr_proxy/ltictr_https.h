
#include "https_tool.h"


int   send_https_response(int sock, SSL* ssl, int num, Buffer* buf);
int   send_https_error(int sock, SSL* ssl, int err);

char* get_https_username(char* path);

//int  recv_https_request(int sock, SSL* ssl, tList** lst, Buffer* buf);
//int  get_tcp_socket(int* port);