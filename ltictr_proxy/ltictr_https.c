
#include "ltictr_https.h"


#define  LTICTR_MIN_PORT    48000
#define  LTICTR_MAX_PORT    50000



int  recv_https_request(int sock, SSL* ssl, tList** lst, Buffer* buf)
{
    int   len, hsz, csz;
    int   connect;
    int   tsecond = 30;
    Buffer cnt;

    if (lst==NULL || buf==NULL) return 0;

    // Receive Header
    hsz = recv_https_header(sock, ssl, lst, &len, NULL, &connect);
    if (hsz<=0 || len==0 || len==HTTP_HEADER_UNKNOWN_LEN) {
        del_tList(lst);
        return -400;        // 400: Bad Request
    }

    // ヘッダ中に紛れ込んだコンテンツの取り出し
    *buf = make_Buffer(RECVBUFSZ);
    cnt  = search_protocol_header(*lst, (char*)HDLIST_CONTENTS_KEY, 1);
    if (cnt.buf!=NULL) {
        csz = cnt.vldsz;
        hsz = hsz - csz;
        copy_Buffer(&cnt, buf);
        free_Buffer(&cnt);
    }
    
    // コンテンツの受信
    int cc = 0;
    if (connect) {
        if (len>0) {
            cc = recv_https_content(sock, ssl, buf, len, tsecond, NULL, &connect);
        }
        else if (len==HTTP_HEADER_CHUNKED) {
            cc = recv_https_chunked(sock, ssl, buf, tsecond, NULL, &connect);
        }
        else { //if (len==HTTP_HEADER_CLOSED_SESSION) {
            cc = recv_https_closed(sock, ssl, buf, tsecond, NULL);
            connect = FALSE;
        }
    }
    //
    if (cc<0 || connect) {
        del_tList(lst);
        free_Buffer(buf);
        return -400;        // 400: Bad Request
    }

    return buf->vldsz;
}



int  send_https_response(int sock, SSL* ssl, int num, Buffer* buf)
{
    tList* hdr = NULL;
    tList* lst = NULL;

    if (num==200) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 200 OK");
        hdr = add_tList_node_str(hdr, "Content-Type", "application/json");
        hdr = add_tList_node_str(hdr, "Content-Length", "0");
    }
    else if (num==201) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 201 Created");
    }
    else if (num==204) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 204 Not Content");
    }

    hdr = add_tList_node_str(hdr, "Connection", "keep-alive");
    char* date = get_http_header_date(time(0));
    if (date!=NULL) {
        hdr = add_tList_node_str(hdr, "Date", date);
        free(date);
    }

    int cc = send_https_Buffer(sock, ssl, lst, buf);

    del_tList(&lst);
    return cc;
}



int  send_https_error(int sock, SSL* ssl, int err)
{
    tList* hdr = NULL;
    tList* lst = NULL;

    if (err<0) err = -err;

    if (err==404) {
        lst = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 404 Not Found");
    }
    else {
        lst = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 400 Bad Request");
    }

    hdr = lst;
    hdr = add_tList_node_str(hdr, "Connection", "close");

    int cc = send_https_header(sock, ssl, lst, OFF);
    del_tList(&lst);

    return cc;
}



int  get_tcp_socket(int* port)
{
    int sock = tcp_server_socket(*port);
    //
    while (sock<=0 && *port<LTICTR_MAX_PORT) {
        (*port)++;
        sock = tcp_server_socket(*port);
    }

    if (sock<=0 && *port>=LTICTR_MAX_PORT) {
        *port = LTICTR_MIN_PORT;
        sock = tcp_server_socket(*port);
        while (sock<0 && *port<LTICTR_MAX_PORT) {
            (*port)++;
            sock = tcp_server_socket(*port);
        }
    }

    return sock;
}
