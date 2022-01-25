

#include "ltictr_http.h"
#include "tjson.h"


//
//
int  send_http_response(int sock, SSL* ssl, int num, Buffer* buf)
{
    tList* hdr = NULL;
    tList* lst = NULL;

    if (num==200) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 200 OK");
        lst = add_tList_node_str(lst, "Content-Type", "application/json");
        lst = add_tList_node_str(lst, "Content-Length", "0");
    }
    else if (num==201) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 201 Created");
        lst = add_tList_node_str(lst, "Content-Length", "0");
    }
    else if (num==204) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 204 Not Content");
    }

    //lst = add_tList_node_str(lst, "Connection", "keep-alive");
    lst = add_tList_node_str(lst, "Connection", "close");
    char* date = get_http_header_date(time(0));
    if (date!=NULL) {
        lst = add_tList_node_str(lst, "Date", date);
        free(date);
    }

    int cc = send_https_Buffer(sock, ssl, hdr, buf);

    DEBUG_MODE {
        print_message("\n=== HTTP SEND ===\n");
        print_protocol_header(hdr);
    }

    del_tList(&hdr);
    return cc;
}



int  send_http_error(int sock, SSL* ssl, int err, Buffer* opt)
{
    tList* hdr = NULL;
    tList* lst = NULL;

    if      (err==400) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 400 Bad Request");
    }
    else if (err==404) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 404 Not Found");
    }
    else if (err==405) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 405 Method Not Allowed");
        if (opt!=NULL) {
            lst = add_tList_node_str(lst, "Allow",  (char*)opt->buf);
        }
    }
    else if (err==500) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 500 Internal Server Error");
    }
    else {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 400 Bad Request");
    }
    //
    lst = add_tList_node_str(lst, "Connection", "close");
 
    int cc = send_https_header(sock, ssl, hdr, OFF);
    del_tList(&hdr);

    return cc;
}



#define  LTICTR_HTTPS_HUB   "/hub/"
#define  LTICTR_HTTPS_USER  "/user/"


char*  get_username_client(tList* hdr)
{
    if (hdr==NULL) return NULL;

    char*  path = NULL;
    Buffer hbuf = search_protocol_header(hdr, (char*)HDLIST_FIRST_LINE_KEY, 1);

    if (hbuf.buf!=NULL) {
        path = cawk((char*)hbuf.buf, ' ', 2);
        if (path!=NULL && *path!='/') {
            free(path);
            path = NULL;
        }
        free_Buffer(&hbuf);
    }
    if (path==NULL) return NULL;

    //
    char* str = NULL;
    char* pp  = strstr(path, LTICTR_HTTPS_HUB);
    //
    if (pp!=NULL) {
        str = dup_str((char*)"/");
    }
    else {
        pp  = strstr(path, LTICTR_HTTPS_USER);
        if (pp!=NULL) {
            pp = pp + strlen(LTICTR_HTTPS_USER);
            char* pt = pp;
            while (*pt!='/' && *pt!='\0') pt++;
            char bkup = *pt;
            *pt = '\0';
            str = dup_str(pp);
            *pt = bkup;
        }
    }
    free(path);

    return str;
}



Buffer  get_user_proxyinfo(char* api_host, int api_port, SSL_CTX* ctx, char* uname, char* token)
{
    Buffer target = init_Buffer();

    if (uname==NULL || api_host==NULL || api_port<=0) return target;

    SSL* ssl = NULL;
    int sofd = tcp_client_socket(api_host, api_port);
    if (sofd<=0) return target;
    if (ctx!=NULL) ssl = ssl_client_socket(sofd, ctx, OFF);

    char get_data[LDATA];
    char get_request[] = "GET /api/routes/user/%s HTTP/1.1";
    snprintf(get_data, LDATA-1, get_request, uname);

    Buffer token_data = make_Buffer_str("token ");
    cat_s2Buffer(token, &token_data);

    tList* http_header = NULL;
    tList* lp = NULL;

    lp = add_tList_node_bystr(lp, 0, 0, HDLIST_FIRST_LINE_KEY, get_data, NULL, 0);
    http_header = lp;
    lp = add_tList_node_bystr(lp, 0, 0, "Host", "", NULL, 0);
    set_http_host_header(lp, api_host, (unsigned short)api_port);
    lp = add_tList_node_bystr(lp, 0, 0, "Accept", "*/*", NULL, 0);
    lp = add_tList_node_bystr(lp, 0, 0, "Connection", "close",  NULL, 0);
    lp = add_tList_node_bystr(lp, 0, 0, "Content-Length", "0", NULL, 0);
    set_protocol_header(lp, "Authorization", (char*)token_data.buf, 1, ON);
    lp = add_tList_node_bystr(lp, 0, 0, HDLIST_END_KEY, "",  NULL, 0);

    send_https_header(sofd, ssl, http_header, OFF);
    del_tList(&http_header);

    Buffer buf = make_Buffer(RECVBUFSZ);
    recv_https_Buffer(sofd, ssl, &http_header, &buf, HTTP_TIMEOUT, NULL, NULL);
    ssl_close(ssl);
    socket_close(sofd);

    tJson* json = json_parse_prop(NULL, (char*)buf.buf, 99);
    free_Buffer(&buf);
    del_tList(&http_header);

    buf = get_key_json_val(json, "user", 1);
    if ((buf.buf!=NULL && !strcmp((char*)buf.buf, uname)) || !strcmp("/", uname)) {
        target = get_key_json_val(json, "target", 1);
    }
    free_Buffer(&buf);
    del_json(&json);

    return target;
}





