/* vi: set tabstop=4 nocindent noautoindent: */


#include "ltictr_xmlrpc.h"


tList*   HTTP_Header  = NULL;
tList*   HTTP_Length  = NULL;
tList*   HTTP_Data    = NULL;


extern char*    Moodle_Host;
extern char*    Moodle_URL;
extern char*    Moodle_Token;
extern char*    Moodle_Service;
extern char*    Moodle_HTTP;
extern int      Moodle_Port;
extern int      Moodle_TLS;
extern int      Moodle_DBAns;



/**
    .......
    init_xml_rpc_header();
    ......
    ......
    post_xml_server(info);  // -> send_xmlrpc_data()
    ......
*/


///////////////////////////////////////////////////////////////////////
// send data to Moodle Web Service

void  send_xmlrpc_data(char* host, int port, int tls, tList* header, int response)
{
    int sock = tcp_client_socket(host, port);
    if (sock<0) {
        DEBUG_MODE {
            print_message("[LTICTR_XMLRPC] Failure to connect to server (%s:%d) : ", host, port);
            jbxl_fprint_state(stderr, sock);
        }
        return;
    }
    SSL_CTX* ctx = NULL;
    SSL* ssl     = NULL;

    if (tls) {
        ctx = ssl_client_setup(NULL);
        ssl = ssl_client_socket(sock, ctx, OFF);
    }

    DEBUG_MODE print_tList(stderr, header);
    send_https_header(sock, ssl, header, ON);
    if (response) { 
        char ans[RECVBUFSZ];
        ssl_tcp_recv(sock, ssl, ans, RECVBUFSZ-1);
        print_message("[LTICTR_XMLRPC] === ANS of XMLRPC ===\n");
        print_message("%s\n\n", ans);
    }

    if (tls) {
        if (ssl!=NULL) ssl_close(ssl);
        SSL_CTX_free(ctx);
    }
    close(sock);

    return;
}



void  post_xmlrpc_server(struct ws_info* info)
{
    tXML* xml = NULL;
    xml = xml_rpc_add_member(xml, "host",     info->host, "");
    xml = xml_rpc_add_member(xml, "inst_id",  info->inst_id, "");
    xml = xml_rpc_add_member(xml, "lti_id",   info->lti_id, "");
    xml = xml_rpc_add_member(xml, "session",  info->session, "");
    xml = xml_rpc_add_member(xml, "message",  info->message, "");
    xml = xml_rpc_add_member(xml, "status",   info->status, "");
    xml = xml_rpc_add_member(xml, "username", info->username, "");
    xml = xml_rpc_add_member(xml, "cell_id",  info->cell_id, "");
    xml = xml_rpc_add_member(xml, "tags",     info->tags, "");
    xml = xml_rpc_add_member(xml, "date",     info->date, "");
    xml = xml_rpc_end_member(xml);
    
    Buffer buf = xml_rpc_request_pack(Moodle_Service, xml);
    copy_i2Buffer((int)buf.vldsz, &(HTTP_Length->ldat.val));
    copy_Buffer(&buf, &(HTTP_Data->ldat.val));

    send_xmlrpc_data(Moodle_Host, Moodle_Port, Moodle_TLS, HTTP_Header, Moodle_DBAns);

    free_Buffer(&buf);
    del_xml(&xml);
}



void  init_xmlrpc_header(void)
{
    char url[LMESG];
    snprintf(url, LMESG-1, "POST %s?wstoken=%s HTTP/%s", Moodle_URL, Moodle_Token, Moodle_HTTP);

    tList* pp = NULL;
    pp = HTTP_Header = add_tList_node_str(pp, HDLIST_FIRST_LINE_KEY, url);
    pp               = add_tList_node_str(pp, "Host", Moodle_Host);
    pp               = add_tList_node_str(pp, "Content-Type", "text/html");
    pp = HTTP_Length = add_tList_node_str(pp, "Content-Length", "");
    pp               = add_tList_node_str(pp, "Connection", "close");
    pp               = add_tList_node_str(pp, HDLIST_END_KEY, "");
    pp = HTTP_Data   = add_tList_node_str(pp, HDLIST_CONTENTS_KEY,  "");

    return;
}

