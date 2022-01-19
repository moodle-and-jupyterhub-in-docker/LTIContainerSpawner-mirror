

#include "ltictr_proxy.h"
#include "tjson.h"
#include "https_tool.h"




struct  ws_info {
    char*  host;
    char*  inst_id;
    char*  lti_id;
    //
    char*  session;
    char*  message;
    char*  status;
    char*  username;
    char*  cell_id;
    char*  tags;
    char*  date;
};



tJson*  ws_json(Buffer* buf, char* mesg, int cc);
tJson*  ws_json_client(char* mesg, int cc);
tJson*  ws_json_server(char* mesg, int cc);


char*  get_sessionid_from_header(char* mesg);
char*  get_info_from_sessioninfo(char* mesg);
char*  get_info_from_cookie(char* mesg);
char*  get_string_from_json(tJson* json);
void   send_data_server();
void   post_xml_server(struct ws_info* info);
void   init_xml_rpc_header(void);
int    init_main(int mode, tList* flist);
int    term_main(void);
int    init_process(int dummy, char* client);
int    term_process(int dummy);
int    fe_server(int dummy1, int sofd, SSL* dummy2, SSL* ssl, char* mesg, int cc);
int    fe_client(int dummy1, int cofd, SSL* dummy2, SSL* ssl, char* mesg, int cc);
