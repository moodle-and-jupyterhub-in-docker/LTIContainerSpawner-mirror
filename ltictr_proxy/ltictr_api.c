
#include "ltictr_api.h"
#include "ltictr_signal.h"
#include "tjson.h"



int  api_main_process(int sock, SSL* ssl, tList* lproxy)
{
    tList* hdr = NULL;             // 受信ヘッダ
    Buffer buf = init_Buffer();    // 受信ボディ

    //
    buf = make_Buffer(RECVBUFSZ);
    int ret = recv_https_Buffer(sock, ssl, &hdr, &buf, HTTP_TIMEOUT, NULL, NULL);
    if (ret<=0) {           // 0 は正常切断
        del_tList(&hdr);
        free_Buffer(&buf);
        if (ret<0) send_http_error(sock, ssl, 400, NULL);
        return -1;          // -1 は切断
    }
    int com = get_http_header_method(hdr);

    //
    DEBUG_MODE {
        print_message("[LTICTR_API] === API RECV ===\n");
        Buffer hbuf = search_protocol_header(hdr, (char*)HDLIST_FIRST_LINE_KEY, 1);
        print_message("[LTICTR_API] %s\n", (char*)hbuf.buf);
        free_Buffer(&hbuf);
    }

    // Get User Name
    char* uname = get_username_api(hdr);
    del_tList(&hdr);
    if (uname==NULL) {
        free_Buffer(&buf);
        if (com<=HTTP_UNKNOWN_METHOD) send_http_error(sock, ssl, 400, NULL);
        else                          send_http_error(sock, ssl, 404, NULL);
        return -1;
    }

    // HTTP Method
    int err = 0;
    // GET
    if (com==HTTP_GET_METHOD) {
        Buffer res = init_Buffer();
        ret = get_user_api(uname, &res, lproxy);
        if (ret==0) {
            send_http_response(sock, ssl, 200, &res);
        }
        else {
            free_Buffer(&res);
            send_http_error(sock, ssl, ret, NULL);
            err = -1;
        }
        free_Buffer(&res);
    }
    // POST
    else if (com==HTTP_POST_METHOD) {
        ret = add_user_api(uname, buf, lproxy);
        if (ret==0) {
            send_http_response(sock, ssl, 201, NULL);
        }
        else {
            err = -1;
            send_http_error(sock, ssl, ret, NULL);
        }
    }
    // DELETE
    else if (com==HTTP_DELETE_METHOD) {
        ret = del_user_api(uname, lproxy);
        if (ret==0) {
            send_http_response(sock, ssl, 204, NULL);
        }
        else {
            err = -1;
            send_http_error(sock, ssl, ret, NULL);
        }
    }
    else {
        print_message("[LTICTR_API] Not Supported Method : %d \n", com);
        err = -1;
        Buffer opt = make_Buffer_str("GET, POST, DELETE");
        send_http_error(sock, ssl, 405, &opt);          // Method Not Allowed
    }

    //
    free(uname);
    free_Buffer(&buf);
    //
    return err;
}



#define  LTICTR_API_ROUTES  "/api/routes"
#define  LTICTR_API_USER    "/api/routes/user/"


char*  get_username_api(tList* hdr)
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

    char* str = NULL;
    char* pp = strstr(path, LTICTR_API_ROUTES);

    if (pp!=NULL) {
        int len = strlen(LTICTR_API_ROUTES);
        if ((pp[len]=='\0') ||  (pp[len]=='/' && pp[len+1]=='\0')) {
            str = dup_str((char*)"/");
        }
        else if (pp[len] == '/') {
            pp = strstr(path, LTICTR_API_USER);
            if (pp!=NULL) {
                len = strlen(LTICTR_API_USER);
                str = dup_str(pp+len);
            }
        }
    }

    free(path);
    return str;
}



//
// GET よる ユーザ uname の問い合わせの返答データを buf に格納する．
//
int  get_user_api(char* uname, Buffer* buf, tList* lproxy)
{
    char json_data[LDATA]; 
    char json_root_fmt[] = "{\"/\":{\"hub\":true,\"target\":\"%s:%d\",\"jupyterhub\":true,\"last_activity\":\"%s\"}}";
    char json_user_fmt[] = "{\"/user/%s\":{\"user\":\"%s\",\"server_name\":\"\",\"target\":\"%s:%d\",\"jupyterhub\":true,\"last_activity\":\"%s\"}}";

    *buf = init_Buffer();
    tList* pp  = NULL;
    tJson* res = json_parse_prop(NULL, "{}", 0);

    if (lproxy!=NULL && lproxy->ldat.id==TLIST_ANCHOR_NODE) lproxy = lproxy->next;

    if (!strcmp("/", uname)) {
        pp = lproxy;
        while (pp!=NULL) {
            char* user = (char*)pp->ldat.key.buf;
            if (user!=NULL) {
                char* url  = (char*)pp->ldat.val.buf;
                //char* date = (char*)pp->ldat.ptr;
                char* date = get_local_timestamp(time(0), "%Y-%b-%dT%H:%M:%SZ");
                int   port = pp->ldat.lv;
                memset(json_data, 0, LDATA);

                if (!strcmp("/", user)) {
                    snprintf(json_data, LDATA-1, json_root_fmt, url, port, date);
                }
                else {
                    snprintf(json_data, LDATA-1, json_user_fmt, user, user, url, port, date);
                }
                tJson* json = json_parse_prop(NULL, json_data, 2);
                insert_json_nodes(res, json);
                free(date);
            }
            pp = pp->next;
        }
    }
    else {
        pp = strncasecmp_tList(lproxy, uname, 0, 1);
        if (pp!=NULL) {
            //
            char* user = (char*)pp->ldat.key.buf;
            char* url  = (char*)pp->ldat.val.buf;
            //char* date = (char*)pp->ldat.ptr;
            char* date = get_local_timestamp(time(0), "%Y-%b-%dT%H:%M:%SZ");
            int   port = pp->ldat.lv;
            memset(json_data, 0, LDATA);
            //
            snprintf(json_data, LDATA-1, json_user_fmt, user, user, url, port, date);
            tJson* json = json_parse_prop(NULL, json_data, 2);
            insert_json_nodes(res, json);
            free(date);
        }
    }

    *buf = json_inverse_parse(res, JSON_ONELINE_FORMAT);
    del_json(&res);

    int ret = 0;
    if (buf->buf==NULL) ret = 500;
    return ret;
}



//
// ユーザ uname を lproxy リストに追加．
// buf : POST データ
//
/**
{
  "user": "bob", 
  "server_name": "", 
  "target": "http://127.0.0.1:49168", 
  "jupyterhub": true
}
*/
int  add_user_api(char* uname, Buffer buf, tList* lproxy)
{
    Buffer target = init_Buffer();
    Buffer protocol, srvfqdn;
    unsigned short int port;

    tJson* json = json_parse_prop(NULL, (char*)buf.buf, 2);
    if (json==NULL) return 400;    // analyze error

    // user name check
    if (strcmp("/", uname)) {
        Buffer user = get_key_json_val(json, "user", 1);
        if (user.buf==NULL || strcmp((char*)uname, (char *)user.buf)) {
            del_json(&json);
            free_Buffer(&user);
            return 400;                // not match
        }
        free_Buffer(&user);
    }
 
    DEBUG_MODE {
        print_message("[LTICTR_API] Add User (%s)\n", uname);
        print_json(stderr, json, JSON_INDENT_FORMAT);
    }
    //
    target = get_key_json_val(json, "target", 1);
    del_json(&json);
    if (target.buf==NULL) return 400;

    decomp_url(target, NULL, &protocol, &srvfqdn, &port, NULL);
    free_Buffer(&target);
    cat_s2Buffer("://",  &protocol);
    cat_Buffer(&srvfqdn, &protocol);

    // check of user exist
    tList* exist = strncasecmp_tList(lproxy, uname, 0, 1);
    if (exist!=NULL) {
        if (exist->ldat.id>0) socket_close(exist->ldat.id);
        del_tList_node(&exist);
    }
    lproxy = find_tList_end(lproxy);
    add_tList_node_bystr(lproxy, 0, (int)port, uname, (char*)protocol.buf, NULL, 0);
    free_Buffer(&srvfqdn);
    free_Buffer(&protocol);

    //
    DEBUG_MODE {
        print_message("[LTICTR_API] Add User (%s)\n", uname);
        print_tList(stderr, lproxy);
        print_message("\n");
    }

    return 0;
}


//
// ユーザ uname を lproxy リストから削除．
//
int  del_user_api(char* uname, tList* lproxy)
{
    tList* pp = strncasecmp_tList(lproxy, uname, 0, 1);
    if (pp==NULL) return 404;      // user does not exist

    del_tList_node(&pp);
    
    //
    DEBUG_MODE {
        print_message("[LTICTR_API] Delete user (%s)\n", uname);
        print_tList(stderr, lproxy);
        print_message("\n");
    }

    return 0;
}



////////////////////////////////////////////////////////////////////////
// HTTP
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


