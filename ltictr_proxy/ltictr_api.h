
#include "ltictr_https.h"


void    api_process(int sock, SSL* ssl);
char*   get_username_api(char* path);

Buffer  get_info_api(char* uname);
Buffer  add_user_api(char* uname);
Buffer  del_user_api(char* uname);

