
#include "ltictr_api.h"


#define  LTICTR_API_ROUTES  "/api/routes"
#define  LTICTR_API_USER    "/api/routes/user/"


char  root_format[] = "{\"/\":{\"hub\":true,\"target\":\"http://%s:%d\",\"jupyterhub\":true,\"last_activity\":\"%s\"}}";
char  user_format[] = "{\"user\": \"%s\", \"server_name\": \"\", \"target\": \"http://%s:%d\", \"jupyterhub\": true}";


void  api_process(int sock, SSL* ssl)
{
    int   cc;
    char* path  = NULL;
    char* uname = NULL;

    tList* lst  = NULL;
    Buffer buf;
    Buffer res;

    //
    cc = recv_https_request(sock, ssl, &lst, &buf);
    if (cc<0 || lst==NULL ) {
        free_Buffer(&buf);
        if (cc<0) send_https_error(sock, ssl, -cc);
        else      send_https_error(sock, ssl, 400);
        return;
    }

    //
    int com = get_http_header_method(lst, &path);
    if (path==NULL) {
        free_Buffer(&buf);
        del_tList(&lst);
        send_https_error(sock, ssl, 400);      // Bad Request
        return;
    }
    uname = get_username_api(path);
    if (uname==NULL) {
        free_Buffer(&buf);
        del_tList(&lst);
        free(path);
        send_https_error(sock, ssl, 400);      // Bad Request
        return;
    }
        
    //
    // GET
    if (com==HTTP_GET_METHOD) {
        res = get_info_api(uname);
        send_https_response(sock, ssl, 200, &res);
    }
    // POST
    else if (com==HTTP_POST_METHOD) {
        res = add_user_api(uname);
        send_https_response(sock, ssl, 201, &res);
    }
    // DELETE
    else if (com==HTTP_DELETE_METHOD) {
        res = del_user_api(uname);
        send_https_response(sock, ssl, 204, &res);
    }
    else {
        print_message("Not Supported Method : %d \n", com);
        print_tList(stderr, lst);
        print_message("%s\n", buf.buf);
        send_https_error(sock, ssl, 400);
    }

    //
    free(path);
    free(uname);
    free_Buffer(&buf);
    free_Buffer(&res);
    del_tList(&lst);
    //
    return;
}



char*  get_username_api(char* path)
{
    char* str = NULL;

    char* pp = strstr(LTICTR_API_ROUTES, path);
    if (pp == NULL) return NULL;

    int len = strlen(LTICTR_API_ROUTES);
    if (pp[len] == '\0') {
        str = dup_str((char*)"");
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
    Buffer buf;

    if (uname[0]!='\0') {
        pp = strncasecmp_tList(lst, uname, 0, 1);



    }



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
Buffer  add_user_api(char* uname, Buffer buf, tList* lst)
{
    Buffer target = init_Buffer();
    Buffer srvfqdn;
    unsigned short port;

    tJson* json = json_parse_prop(NULL, buf.buf, 2);
    if (json!=NULL) {
        target = get_key_json_val(json, "target", 1);
        decomp_url(target, NULL, NULL, &srvfqdn, &port, NULL);
    }

    int sock = tcp_client_socket(srvfqdn.buf, port);

    lst = find_tList_end(lst);
    char* get_timestamp(time(0), "%Y-%b-%dT%H:%M:%SZ");
    add_tList_node_bystr(lst, sock, port, uname, , NULL, 0);

    //tList*  find_tList_end(tList* pp);

    return buf;
}





Buffer  del_user_api(char* uname)
{
    Buffer buf;


    return buf;
}



