
#include "ltictr_api.h"
#include "tjson.h"


#define  LTICTR_API_ROUTES  "/api/routes"
#define  LTICTR_API_USER    "/api/routes/user/"



int  api_process(int sock, SSL* ssl, tList* lproxy)
{
    tList* lst = NULL;             // 受信ヘッダ
    Buffer buf = init_Buffer();    // 受信ボディ

    //
    buf = make_Buffer(RECVBUFSZ);
    int ret = recv_https_Buffer(sock, ssl, &lst, &buf, 0, NULL, NULL);
    if (ret<=0) {           // 0 は正常切断
        del_tList(&lst);
        free_Buffer(&buf);
        if (ret<0) send_https_error(sock, ssl, 400);
        return -1;          // -1 は切断
    }

    //
    DEBUG_MODE {
        print_message("\n=== API RECV ===\n");
        print_tList(stderr, lst);
        print_message("%s\n", buf.buf);
    }

    // Get User Name
    char* path  = NULL;
    int   com   = get_http_header_method(lst, &path);    // get http command and path
    char* uname = get_username_api(path);                // get user name from path
    del_tList(&lst);
    if (path!=NULL) free(path);
    if (uname==NULL) {
        free_Buffer(&buf);
        if (com==HTTP_UNKNOWN_METHOD) send_https_error(sock, ssl, 400);
        else                          send_https_error(sock, ssl, 404);
        return -1;
    }

    // HTTP Method
    int err = 0;
    // GET
    if (com==HTTP_GET_METHOD) {
        Buffer res = init_Buffer();
        ret = get_user_api(uname, &res, lproxy);
        if (ret==0) {
            send_https_response(sock, ssl, 200, &res);
        }
        else {
            free_Buffer(&res);
            send_https_error(sock, ssl, ret);
            err = -1;
        }
        free_Buffer(&res);
    }
    // POST
    else if (com==HTTP_POST_METHOD) {
        ret = add_user_api(uname, buf, lproxy);
        if (ret==0) {
            send_https_response(sock, ssl, 201, NULL);
        }
        else {
            err = -1;
            send_https_error(sock, ssl, ret);
        }
    }
    // DELETE
    else if (com==HTTP_DELETE_METHOD) {
        ret = del_user_api(uname, lproxy);
        if (ret==0) {
            send_https_response(sock, ssl, 204, NULL);
        }
        else {
            err = -1;
            send_https_error(sock, ssl, ret);
        }
    }
    else {
        print_message("Not Supported Method : %d \n", com);
        err = -1;
        send_https_error(sock, ssl, 400);
    }

    //
    if (uname!=NULL) free(uname);
    free_Buffer(&buf);
    //
    return err;
}



char*  get_username_api(char* path)
{
    if (path==NULL) return NULL;

    char* str = NULL;
    char* pp = strstr(path, LTICTR_API_ROUTES);
    if (pp == NULL) return NULL;

    int len = strlen(LTICTR_API_ROUTES);
    if ((pp[len]=='\0') ||  (pp[len]=='/' && pp[len+1]=='\0')) {
        str = dup_str((char*)"/");
    }
    else if (pp[len] == '/') {
        pp = strstr(path, LTICTR_API_USER);
        if (pp==NULL) return NULL;
        len = strlen(LTICTR_API_USER);
        str = dup_str(pp+len);
    }

    return str;
}



int  get_user_api(char* uname, Buffer* buf, tList* lst)
{
    char json_data[LDATA]; 
    char json_root_fmt[] = "{\"/\":{\"hub\":true,\"target\":\"%s:%d\",\"jupyterhub\":true,\"last_activity\":\"%s\"}}";
    char json_user_fmt[] = "{\"/user/%s\":{\"user\":\"%s\",\"server_name\":\"\",\"target\":\"%s:%d\",\"jupyterhub\":true,\"last_activity\":\"%s\"}}";

    tList* pp  = NULL;
    tJson* res = json_parse_prop(NULL, "{}", 0);

    if (!strcmp("/", uname)) {
        pp = lst;
        while (pp!=NULL) {
            char* user = (char*)pp->ldat.key.buf;
            if (user!=NULL) {
                char* url  = (char*)pp->ldat.val.buf;
                char* date = (char*)pp->ldat.ptr;
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
            }
            pp = pp->next;
        }
    }
    else {
        pp = strncasecmp_tList(lst, uname, 0, 1);
        //
        char* user = (char*)pp->ldat.key.buf;
        char* url  = (char*)pp->ldat.val.buf;
        char* date = (char*)pp->ldat.ptr;
        int   port = pp->ldat.lv;
        memset(json_data, 0, LDATA);
        //
        snprintf(json_data, LDATA-1, json_user_fmt, user, user, url, port, date);
        tJson* json = json_parse_prop(NULL, json_data, 2);
        insert_json_nodes(res, json);
    }

    *buf = json_inverse_parse(res, JSON_ONELINE_FORMAT);
    del_json(&res);

    int ret = 0;
    if (buf->buf==NULL) ret = 500;
    return ret;
}



/**
{
  "user": "bob", 
  "server_name": "", 
  "target": "http://127.0.0.1:49168", 
  "jupyterhub": true
}
*/
int  add_user_api(char* uname, Buffer buf, tList* lst)
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
 
    //
    target = get_key_json_val(json, "target", 1);
    del_json(&json);
    if (target.buf==NULL) return 400;

    decomp_url(target, NULL, &protocol, &srvfqdn, &port, NULL);
    free_Buffer(&target);
    cat_s2Buffer("://",  &protocol);
    cat_Buffer(&srvfqdn, &protocol);

    // check of user exist
    tList* exist = strncasecmp_tList(lst, uname, 0, 1);
    if (exist!=NULL) {
        if (exist->ldat.id>0) socket_close(exist->ldat.id);
        del_tList_node(&exist);
    }
    // port open
    //int sock = tcp_client_socket((char*)srvfqdn.buf, (int)port);
    //if (sock<=0) {
    //    free_Buffer(&srvfqdn);
    //    free_Buffer(&protocol);
    //    return 500;
    //}
    //
    char* lasttime = get_local_timestamp(time(0), "%Y-%b-%dT%H:%M:%SZ");
    lst = find_tList_end(lst);
    add_tList_node_bystr(lst, 0, (int)port, uname, (char*)protocol.buf, lasttime, strlen(lasttime)+1);
    free_Buffer(&srvfqdn);
    free_Buffer(&protocol);
    free(lasttime);

    //
    DEBUG_MODE {
        print_message("\n=== ADD USER API ===\n");
        print_tList(stderr, lst);
    }

    return 0;
}



int  del_user_api(char* uname, tList* lst)
{
    tList* pp = strncasecmp_tList(lst, uname, 0, 1);
    if (pp==NULL) return 404;      // user does not exist

    socket_close(pp->ldat.id);
    del_tList_node(&pp);
    
    //
    DEBUG_MODE {
        print_message("\n=== DEL USER API === (%s)\n", uname);
        print_tList(stderr, lst);
    }

    return 0;
}


