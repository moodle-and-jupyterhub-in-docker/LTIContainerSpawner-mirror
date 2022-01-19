
#include "ltictr_api.h"
#include "tjson.h"


#define  LTICTR_API_ROUTES  "/api/routes"
#define  LTICTR_API_USER    "/api/routes/user/"



void  api_process(int sock, SSL* ssl, tList* lprxy)
{
    tList* lst  = NULL;     // 受信ヘッダ
    Buffer buf;             // 受信ボディ
    char* uname = NULL;     // ex. bob
    char* path  = NULL;     // ex. /api/routes/user/bob
    int   com;

    //
    int err = recv_https_request(sock, ssl, &lst, &buf);
    //
    DEBUG_MODE {
        print_message("\n=== API RECV ===\n");
        print_tList(stderr, lst);
        print_message("%s\n", buf.buf);
    }
    if (err>0) {
        del_tList(&lst);
        free_Buffer(&buf);
        send_https_error(sock, ssl, err);
        return;
    }

    //
    com   = get_http_header_method(lst, &path);    // get http command and path
    uname = get_username_api(path);                // get user name from path
    if (uname==NULL) err = 400;
    if (err>0) send_https_error(sock, ssl, err);

    free_Buffer(&buf);
    del_tList(&lst);
    if (path!=NULL) free(path);


    //
    // GET
    if (com==HTTP_GET_METHOD) {
        Buffer res = get_info_api(uname, lprxy);
        if (res.state>=0) send_https_response(sock, ssl, 200, &res);
        else              send_https_error(sock, ssl, res.state);
        free_Buffer(&res);
    }
    // POST
    else if (com==HTTP_POST_METHOD) {
        err = add_user_api(uname, buf, lprxy);
        if (err==0) send_https_response(sock, ssl, 201, NULL);
        else        send_https_error(sock, ssl, err);
    }
    // DELETE
    else if (com==HTTP_DELETE_METHOD) {
        err = del_user_api(uname, lprxy);
        if (err==0) send_https_response(sock, ssl, 204, NULL);
        else        send_https_error(sock, ssl, err);
    }
    else {
        print_message("Not Supported Method : %d \n", com);
        send_https_error(sock, ssl, 400);
    }

    //
    free(uname);
    free_Buffer(&buf);
    //del_tList(&lst);
    //
    return;
}




char*  get_username_api(char* path)
{
    if (path==NULL) return NULL;

    char* str = NULL;
    char* pp = strstr(LTICTR_API_ROUTES, path);
    if (pp == NULL) return NULL;

    int len = strlen(LTICTR_API_ROUTES);
    if (pp[len] == '\0') {
        str = dup_str((char*)"hub");
    }
    else if (pp[len] == '/') {
        pp = strstr(LTICTR_API_USER, path);
        if (pp==NULL) return NULL;
        len = strlen(LTICTR_API_USER);
        str = dup_str(pp+len);
    }

    return str;
}



Buffer  get_info_api(char* uname, tList* lst)
{
    char json_data[LDATA]; 
    char json_hub_fmt[]  = "{\"/\":{\"hub\":true,\"target\":\"%s:%d\",\"jupyterhub\":true,\"last_activity\":\"%s\"}}";
    char json_user_fmt[] = "{\"/user/%s\":{\"user\":\"%s\",\"server_name\":\"\",\"target\":\"%s:%d\",\"jupyterhub\":true,\"last_activity\":\"%s\"}}";

    Buffer buf = init_Buffer();
    tJson* res = json_parse_prop(NULL, "{}", 0);
    tList* pp  = NULL;

    if (!strcmp("hub", uname)) {
        pp = lst;
        while (pp!=NULL) {
            char* user = (char*)pp->ldat.key.buf;
            char* url  = (char*)pp->ldat.val.buf;
            char* date = (char*)pp->ldat.ptr;
            int   port = pp->ldat.lv;
            memset(json_data, 0, LDATA);

            if (!strcmp("hub", user)) {
                snprintf(json_data, LDATA-1, json_hub_fmt, url, port, date);
            }
            else {
                snprintf(json_data, LDATA-1, json_user_fmt, user, user, url, port, date);
            }
            tJson* json = json_parse_prop(NULL, json_data, 2);
            insert_json_nodes(res, json);

            pp = pp->next;
        }
    }
    else {
        pp = strncasecmp_tList(lst, uname, 0, 1);

        char* user = (char*)pp->ldat.key.buf;
        char* url  = (char*)pp->ldat.val.buf;
        char* date = (char*)pp->ldat.ptr;
        int   port = pp->ldat.lv;
        memset(json_data, 0, LDATA);

        snprintf(json_data, LDATA-1, json_user_fmt, user, user, url, port, date);

        tJson* json = json_parse_prop(NULL, json_data, 2);
        insert_json_nodes(res, json);
    }

    buf = json_inverse_parse(res, JSON_ONELINE_FORMAT);
    buf.state = JBXL_NORMAL;
    return buf;
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
    Buffer user   = init_Buffer();
    Buffer protocol, srvfqdn;
    unsigned short int port;

    tList* pp = strncasecmp_tList(lst, uname, 0, 1);
    if (pp!=NULL) return 400;      // already exist user

    tJson* json = json_parse_prop(NULL, (char*)buf.buf, 2);
    if (json==NULL) return 400;    // analyze error

    user = get_key_json_val(json, "user", 1);
    if (user.buf==NULL || strcmp((char*)uname, (char *)user.buf)) {
        del_json(&json);
        free_Buffer(&user);
        return 400;                // not match
    }
    free_Buffer(&user);
                
    target = get_key_json_val(json, "target", 1);
    del_json(&json);
    if (target.buf==NULL) return 400;

    decomp_url(target, NULL, &protocol, &srvfqdn, &port, NULL);
    free_Buffer(&target);

    int sock = tcp_client_socket((char*)srvfqdn.buf, (int)port);
    if (sock<=0) {
        free_Buffer(&srvfqdn);
        return 500;
    }

    cat_s2Buffer("://",  &protocol);
    cat_Buffer(&srvfqdn, &protocol);
    free_Buffer(&srvfqdn);

    lst = find_tList_end(lst);
    char* lasttime = get_local_timestamp(time(0), "%Y-%b-%dT%H:%M:%SZ");
    //
    add_tList_node_bystr(lst, sock, (int)port, uname, (char*)protocol.buf, lasttime, strlen(lasttime)+1);
    free(lasttime);
    free_Buffer(&protocol);

    //
    DEBUG_MODE {
        print_message("POST %s\n", uname);
        print_tList(stderr, lst);
    }

    return 0;
}



int  del_user_api(char* uname, tList* lst)
{
    tList* pp = strncasecmp_tList(lst, uname, 0, 1);
    if (pp==NULL) return 400;      // not exist user

    socket_close(pp->ldat.id);
    del_tList_node(&pp);
    
    //
    DEBUG_MODE {
        print_message("DELETE %s\n", uname);
        print_tList(stderr, lst);
    }

    return 0;
}


