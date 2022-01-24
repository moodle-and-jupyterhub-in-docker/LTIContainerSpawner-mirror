

#include "ltictr_http.h"


#define  LTICTR_HTTPS_HUB   "/hub/"
#define  LTICTR_HTTPS_USER  "/user/"



int  send_https_response(int sock, SSL* ssl, int num, Buffer* buf)
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

    lst = add_tList_node_str(lst, "Connection", "keep-alive");
    char* date = get_http_header_date(time(0));
    if (date!=NULL) {
        lst = add_tList_node_str(lst, "Date", date);
        free(date);
    }

    int cc = send_https_Buffer(sock, ssl, hdr, buf);

    DEBUG_MODE {
        print_message("\n=== HTTP SEND ===\n");
        Buffer hbuf = search_protocol_header(hdr, (char*)HDLIST_FIRST_LINE_KEY, 1);
        print_message("%s\n", (char*)hbuf.buf);
        free_Buffer(&hbuf);
    }

    del_tList(&hdr);
    return cc;
}



int  send_https_error(int sock, SSL* ssl, int err, Buffer* opt)
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



char*  get_username_http(tList* hdr)
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
