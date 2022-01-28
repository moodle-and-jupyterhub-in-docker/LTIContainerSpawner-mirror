/* vi: set tabstop=4 nocindent noautoindent: */


#include "ltictr_proxy.h"
#include "ltictr_signal.h"


#define  LTICTR_IDLETIME   900       // 15m
#define  LTICTR_TIMEOUT    30        // 30s

#define  SESSION_ID_KEY    "session_id="
#define  SESSION_INFO_KEY  "lms_sessioninfo="           // Instance id, LTI id


char*    SessionInfo  = NULL;

extern char*  API_Token;


//
void  receipt_proxy(int ssock, SSL_CTX* server_ctx, SSL_CTX* client_ctx, Buffer api_host, tList* lproxy)
{
    int    cc, nd;
    fd_set mask;
    struct timeval timeout;

    Buffer buf   = init_Buffer();   // 受信ボディ
    tList* hdr   = NULL;            // 受信ヘッダ
    tList* lst   = NULL;
    char* sproto = NULL;

    int  csock   = 0;
    SSL* sssl    = NULL;
    SSL* cssl    = NULL;
    
    SSL_CTX* api_ctx = NULL;
    Buffer aserver   = init_Buffer();
    Buffer protocol  = init_Buffer();
    unsigned short aport = 0;

    //
    if (api_host.buf!=NULL) {
        decomp_url(api_host, NULL, &protocol, &aserver, &aport, NULL);
        if (ex_strcmp("https", (char*)protocol.buf)) {
            api_ctx = ssl_client_setup(NULL);
        }
        free_Buffer(&protocol);
    }
    //
    // Client SSL connection for data recieve
    if (server_ctx!=NULL) {
        sproto = dup_str("https");
        sssl = ssl_server_socket(ssock, server_ctx);
        if (sssl==NULL) {
            free(sproto);
            close(ssock);
            print_message("[LTICTR_PROXY] Failure to create the server SSL socket. (%d)\n", getpid());
            sig_term(-1);
        }
        DEBUG_MODE print_message("[LTICTR_PROXY] Opened socket for SSL server. (%d)\n", getpid());
    }
    else sproto = dup_str("http");
    
    //
    buf = make_Buffer(RECVBUFSZ);
    int range = ssock;
    timeout.tv_sec  = LTICTR_IDLETIME;
    timeout.tv_usec = 0;
    FD_ZERO(&mask); 
    FD_SET(ssock, &mask);
    nd = select(range+1, &mask, NULL, NULL, &timeout);

    // Main Loop
    DEBUG_MODE print_message("[LTICTR_PROXY] Start Main Loop. (%d)\n", getpid());
    while(nd>0) {
        //////////////////////////////////////////////////////////////////////////////////////////
        // Client -> Server // ltictr_proxy_server はサーバ
        if (FD_ISSET(ssock, &mask)) {
            cc = recv_https_Buffer(ssock, sssl, &hdr, &buf, LTICTR_TIMEOUT, NULL, NULL);
            if (cc>0) {
                DEBUG_MODE {
                    print_message("[LTICTR_PROXY] === HTTP RECV CLIENT ===\n");
                    print_protocol_header(hdr);
                    print_message("\n");
                }
                //
                lst = NULL;
                char* uname = NULL;
                if (aport>0) {
                    uname = get_proxy_username(hdr);
                    if (uname!=NULL) {
                        lst = strncasecmp_tList(lproxy, uname, 0, 1);
                        if (lst==NULL) {
                            Buffer target = get_proxy_target((char*)aserver.buf, (int)aport, api_ctx, uname, API_Token);
                            if (target.buf!=NULL) {
                                char* pp = (char*)target.buf;
                                char* pt = pp + strlen((char*)target.buf);
                                while (*pt!=':') pt--;
                                *pt = '\0';
                                pt++;
                                lst = add_tList_node_bystr(lproxy, 0, atoi(pt), uname, pp, NULL, 0);
                                free_Buffer(&target);
                            }
                        }
                        free(uname);
                    }
                }
                if (lst==NULL) {
                    lst = strncasecmp_tList(lproxy, "/", 0, 1);
                }

                csock = get_proxy_socket(lst);
                cssl  = get_proxy_ssl(csock, client_ctx, lst);
                cc = send_server(csock, cssl, hdr, buf, sproto);     // Server へ転送
                del_tList(&hdr);
                if (cc<=0) break;
            }
            else {
                del_tList(&hdr);
                break;      // cc==0
            }
        }

        //
        //////////////////////////////////////////////////////////////////////////////////////////
        range = ssock;
        timeout.tv_sec  = LTICTR_TIMEOUT;
        timeout.tv_usec = 0;
        FD_ZERO(&mask); 
        FD_SET(ssock, &mask);
        //
        lst = lproxy;
        if (lst->ldat.id==TLIST_ANCHOR_NODE) lst = lst->next;
        while (lst!=NULL) {
            csock = lst->ldat.id;
            if (csock>0) {
                FD_SET(csock, &mask);
                range = Max(range, csock);
            }
            lst = lst->next;
        }
        nd = select(range+1, &mask, NULL, NULL, &timeout);

        //////////////////////////////////////////////////////////////////////////////////////////
        // Server -> Client // ltictr_proxy_erver はクライアント
        lst = lproxy;
        if (lst->ldat.id==TLIST_ANCHOR_NODE) lst = lst->next;
        while (lst!=NULL) {
            csock = lst->ldat.id;
            if (csock>0) {
                if (FD_ISSET(csock, &mask)) {
                    cssl = get_proxy_ssl(csock, client_ctx, lst);
                    cc = recv_https_Buffer(csock, cssl, &hdr, &buf, LTICTR_TIMEOUT, NULL, NULL); 
                    if (cc>0) {
                        DEBUG_MODE {
                            print_message("[LTICTR_PROXY] === HTTP RECV SERVER === (%d)\n", csock);
                            print_protocol_header(hdr);
                            print_message("\n");
                        }
                        cc = send_client(ssock, sssl, hdr, buf);     // Client へ転送
                        if (cc<=0) {
                            ssl_close(cssl);
                            close(csock);
                            lst->ldat.id = 0;
                            if (lst->ldat.ptr!=NULL) lst->ldat.ptr = NULL;
                            lst->ldat.sz = 0;
                            //break;
                        }
                    }
                    else {
                        ssl_close(cssl);
                        close(csock);
                        lst->ldat.id = 0;
                        if (lst->ldat.ptr!=NULL) lst->ldat.ptr = NULL;
                        lst->ldat.sz = 0;
                        //break;      // cc==0
                    }
                    del_tList(&hdr);
                }
            }
            lst = lst->next;
        }

        //
        //////////////////////////////////////////////////////////////////////////////////////////
        range = ssock;
        timeout.tv_sec  = LTICTR_TIMEOUT;
        timeout.tv_usec = 0;
        FD_ZERO(&mask); 
        FD_SET(ssock, &mask);
        //
        lst = lproxy;
        if (lst->ldat.id==TLIST_ANCHOR_NODE) lst = lst->next;
        while (lst!=NULL) {
            csock = lst->ldat.id;
            if (csock>0) {
                FD_SET(csock, &mask);
                range = Max(range, csock);
            }
            lst = lst->next;
        }
        nd = select(range+1, &mask, NULL, NULL, &timeout);
    }
    DEBUG_MODE print_message("[LTICTR_PROXY] Stop  Main Loop. (%d)\n", getpid());

    ssl_close(sssl);
    if (sssl==NULL) close(ssock);

    lst = lproxy;
    if (lst->ldat.id==TLIST_ANCHOR_NODE) lst = lst->next;
    while (lst!=NULL) {
        csock = lst->ldat.id;
        if (csock>0) {
            if (lst->ldat.ptr!=NULL) {
                ssl_close((SSL*)lst->ldat.ptr);
                lst->ldat.ptr = NULL;
            }
            lst->ldat.sz = 0;
            lst->ldat.id = 0;
            socket_close(csock);
        }
        lst = lst->next;
    }
    if (api_ctx!=NULL) SSL_CTX_free(api_ctx);

    free(sproto);

    DEBUG_MODE print_message("[LTICTR_PROXY] Termination of Process. (%d)\n", getpid());
    _exit(0);
}


//
//    Server -> Client 
//
int   send_client(int sock, SSL* ssl, tList* hdr, Buffer buf)
{
    if (hdr==NULL) return -1;

    int http_com = get_http_header_method(hdr);
    //int http_res = OFF;

    if (http_com > HTTP_UNKNOWN_METHOD) {

        char*  resp = NULL;
        tList* lp = search_key_tList(hdr, HDLIST_FIRST_LINE_KEY, 1);
        if (lp!=NULL) resp = (char*)lp->ldat.val.buf;

        // add cookie
        // Session Info を lms_sessioninfo の値として cookie に追加
        if (SessionInfo!=NULL && resp!=NULL && ex_strcmp("HTTP/", resp)) {
            //http_res = ON;
            lp = search_key_tList(hdr, "Set-Cookie", 1);
            if (lp==NULL) lp = search_key_tList(hdr, "Host", 1);
            //
            char cookie[LMESG];
            snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", SESSION_INFO_KEY, SessionInfo);
            add_protocol_header(lp, "Set-Cookie", cookie);
            free(SessionInfo);
            SessionInfo = NULL;
        }
    }


    //////////////////////////////////////////////////////////
    Buffer snd = rebuild_http_Buffer(hdr, &buf);
    int ret = ssl_tcp_send(sock, ssl, (char*)snd.buf, snd.vldsz);
    free_Buffer(&snd);
    //////////////////////////////////////////////////////////

    if (http_com>HTTP_UNKNOWN_METHOD) {

    //
    // Web Socket
    static char host[] = "server";

    tJson* temp = NULL;
    tJson* json = NULL;
    //if (*(unsigned char*)mesg==0x81) json = ws_json(mesg, cc);
    if (json!=NULL) {
        //print_json(stderr, json);
        struct ws_info info;
        memset(&info, 0, sizeof(struct ws_info));
        //
        char* type = get_string_from_json(search_key_json(json, "msg_type", TRUE, 1));
        if (type!=NULL && ex_strcmp("execute_reply", type)) { 
            info.status = get_string_from_json(find_double_key_json(json, "content", "status"));
            if (info.status!=NULL) {
                temp = find_double_key_json(json, "header", "username");
                info.username = get_string_from_json(temp);
                if (info.username!=NULL) {
                    info.date = get_string_from_json(find_key_sister_json(temp, "date"));
                    temp = find_double_key_json(json, "parent_header", "session");
                    info.session = get_string_from_json(temp);
                    if (info.session!=NULL) {
                        info.message = get_string_from_json(find_key_sister_json(temp, "msg_id"));
                        info.host    = host;
                        post_xmlrpc_server(&info);
                        //
                        if (info.message!=NULL) free(info.message);
                        free(info.session);
                    }
                    if (info.date!=NULL) free(info.date);
                    free(info.username);
                }
                free(info.status);
            }
            free(type);
        }
        del_json(&json);
    }
    }

    return ret;
}



//
//    Client -> Server 
//
int   send_server(int sock, SSL* ssl, tList* hdr, Buffer buf, char* proto)
{
    if (sock<=0 || hdr==NULL) return -1;

    int http_com = get_http_header_method(hdr);
    //
    tList* ph = search_key_tList(hdr, "Host", 1);
    add_protocol_header(ph, "X-Forwarded-Proto", proto);

//print_message("++++> SEND SERVER \n");
//print_tList(stderr, hdr);
//print_message("%s\n", (char*)buf.buf);
    //////////////////////////////////////////////////////////
    Buffer snd = rebuild_http_Buffer(hdr, &buf);
    int cc = ssl_tcp_send(sock, ssl, (char*)snd.buf, snd.vldsz);
    free_Buffer(&snd);
    //////////////////////////////////////////////////////////

    static char ltictr[] = "ltictr";
    //
    // GET session_id と cookie の lms_sessionifo (course_id+%2C+lti_id) を関連付けて XMLRPC で送る．
    if (http_com == HTTP_GET_METHOD) {
        //content_length = 0;
        //recv_buffer = init_Buffer();
        //
        char* sessionid = get_sessionid_from_header(hdr);   // URL パラメータから session_id を得る
        if (sessionid!=NULL) {
            char* ssninfo = get_info_from_cookie(hdr);          // ヘッダから Cookie を得る
            if (ssninfo!=NULL) {
                struct ws_info info;
                memset(&info, 0, sizeof(struct ws_info));
                //
                char* pt = ssninfo;
                while (*pt!='%' && *pt!='\0') pt++;
                if (*pt=='%') {
                    *pt = '\0';
                    pt = pt + 3;
                }
                info.host    = ltictr;
                info.inst_id = ssninfo;
                info.lti_id  = pt;
                info.session = sessionid;
                //
                post_xmlrpc_server(&info);
                //
                free(ssninfo);
                free(sessionid);
            }
        }
    }

    //
    else if (http_com == HTTP_POST_METHOD) {
        if (SessionInfo==NULL) {
            if (ex_strcmp("oauth_version", (char*)buf.buf)) {
                SessionInfo = get_info_from_sessioninfo((char*)buf.buf);  
            }
            // 
            //if (SessionInfo==NULL && strstr(buf.buf,  SESSION_INFO_KEY) != NULL) {
            //    SessionInfo = get_info_from_sessioninfo((char*)buf.buf);  
            //}
        }
    }


    else {
    //
    // Web Socket
    static char host[]  = "client";

    tJson* temp = NULL;
    tJson* json = NULL;
    //if (*(unsigned char*)mesg==0x81) json = ws_json(mesg, cc);
    if (http_com==0) json = ws_json_client((char*)buf.buf, buf.vldsz);
    if (json!=NULL) {
        struct ws_info info;
        memset(&info, 0, sizeof(struct ws_info));
        //
        temp = find_double_key_json(json, "metadata", "cellId");
        info.cell_id = get_string_from_json(temp);
        if (info.cell_id!=NULL) {
            info.tags = get_string_from_json(find_key_sister_json(temp, "tags"));
            temp = find_double_key_json(json, "header", "session");
            info.session = get_string_from_json(temp);
            if (info.session!=NULL) {
                info.date     = get_string_from_json(find_key_sister_json(temp, "date"));
                info.message  = get_string_from_json(find_key_sister_json(temp, "msg_id"));
                info.host     = host;
                post_xmlrpc_server(&info);
                //
                if (info.message!=NULL) free(info.message);
                if (info.date   !=NULL) free(info.date);
                free(info.session);
            }
            if (info.tags!=NULL) free(info.tags);
            free(info.cell_id);
        }
        del_json(&json);
    }
    }

    //
    return cc;
}



int  get_proxy_socket(tList* lst)
{
    int sock = 0;
    if (lst==NULL) return -1;
    //
    sock = (int)lst->ldat.id;
    if (sock<=0) {
        DEBUG_MODE print_message("[LTICTR_PROXY] Socket for %s is invalid. Reopen socket. (%d)\n", (char*)lst->ldat.key.buf, sock);
        //
        char* hp = (char*)lst->ldat.val.buf + lst->ldat.val.vldsz;
        while(*hp!='/') hp--;
        char* hname = dup_str(hp + 1);
        sock = tcp_client_socket(hname, lst->ldat.lv);
        free(hname);
        //
        if (sock>0) {
            lst->ldat.id = sock;
        }
        else sock = -1;
    }

    return sock;
}



SSL*  get_proxy_ssl(int sock, SSL_CTX* ctx, tList* lst)
{
    SSL* ssl = NULL;
    if (sock<=0 || lst==NULL) return NULL;
    //
    if (ex_strcmp("https:", (char*)lst->ldat.val.buf)) {
        if (lst->ldat.ptr!=NULL) {
            ssl = (SSL*)lst->ldat.ptr;
        }
        else {
            ssl = ssl_client_socket(sock, ctx, OFF);
            if (ssl!=NULL) {
                lst->ldat.ptr = (void*)ssl;
                lst->ldat.sz  = sizeof(SSL*);
            }
            else {
                DEBUG_MODE print_message("[LTICTR_PROXY] Failure to connect to server SSL port. (%d)\n", getpid());
                sig_term(-1);
            }
        }
    }
    return ssl;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define  LTICTR_HTTPS_HUB   "/hub/"
#define  LTICTR_HTTPS_USER  "/user/"


char*  get_proxy_username(tList* hdr)
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
    char* unm = NULL;

    if (ex_strcmp(LTICTR_HTTPS_USER, path)) {
        char* pp = path + strlen(LTICTR_HTTPS_USER);
        char* pt = pp;
        while (*pt!='/' && *pt!='\0') pt++;
        *pt = '\0';
        unm = dup_str(pp);
    }
    //else if (ex_strcmp(LTICTR_HTTPS_HUB, path)) {
    //    unm = dup_str("/");
    //}
    else {
        unm = dup_str("/");
    }
    free(path);

    return unm;
}



Buffer  get_proxy_target(char* api_host, int api_port, SSL_CTX* ctx, char* uname, char* token)
{
    char localip[] = "127.0.0.1";

    Buffer target = init_Buffer();
    if (api_host==NULL || api_port<=0 || uname==NULL) return target;
    if (api_host[0]=='\0') api_host = localip;

    SSL* ssl = NULL;
    int sofd = tcp_client_socket(api_host, api_port);
    if (sofd<=0) return target;
    if (ctx!=NULL) {
        ssl = ssl_client_socket(sofd, ctx, OFF);
    }

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
    lp = add_tList_node_bystr(lp, 0, 0, "Authorization", (char*)token_data.buf, NULL, 0);
    lp = add_tList_node_bystr(lp, 0, 0, HDLIST_END_KEY, "",  NULL, 0);
    free_Buffer(&token_data);

    send_https_header(sofd, ssl, http_header, OFF);
    del_tList(&http_header);

    Buffer buf = make_Buffer(RECVBUFSZ);
    recv_https_Buffer(sofd, ssl, &http_header, &buf, LTICTR_TIMEOUT, NULL, NULL);
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



///////////////////////////////////////////////////////////////////////
// get information from HTTP

//
// URL パラメータから，存在するならば セッションID を取り出す．
// SESSION_ID_KEY はクライアント（Webブラウザ）からのリクエスト中のボディデータ中に設定されている値．
// 要 free
//
char*  get_sessionid_from_header(tList* hdr)
{
    if (hdr==NULL) return NULL;
    //
    tList* lp = search_key_tList(hdr, HDLIST_FIRST_LINE_KEY, 1);
    if (lp==NULL)  return NULL;
    char* url = dup_str((char*)lp->ldat.val.buf);
    if (url==NULL) return NULL;

    char* pp = strstr(url, SESSION_ID_KEY);
    if (pp==NULL) {
        free(url);
        return NULL;
    }
    pp = pp + strlen(SESSION_ID_KEY);

    char* pt = pp;
    while(*pt!=' ' && *pt!='&' && *pt!='%' && *pt!='\0') pt++;

    *pt = '\0';
    char* sid = dup_str(pp);

    free(url);
    return sid;
}


//
// クライアント（Webブラウザ）のクッキーから コースIDとLTIのインスタンスID を取り出す．
// 要 free
//
char*  get_info_from_cookie(tList* hdr)
{
    if (hdr==NULL) return NULL;

    tList* lp = search_key_tList(hdr, "Cookie", 1);
    if (lp==NULL)  return NULL;
    char* cke = dup_str((char*)lp->ldat.val.buf);
    if (cke==NULL) return NULL;

    char* pp = strstr(cke, SESSION_INFO_KEY);
    if (pp==NULL) {
        free(cke);
        return NULL;
    }
    pp = pp + strlen(SESSION_INFO_KEY);

    char* pt = pp; 
    while (*pt!=';' && *pt!='\0') pt++;

    *pt = '\0';
    char* ssninfo = dup_str(pp);

    free(cke);
    return ssninfo;
}



//
// HTTPのPOST受信データ（mesg）のボディから コースIDとLTIのインスタンスID を取り出す．
// mesg は LTI コンシューマ（クライアント: Moodle）からのデータ．
// 要 free
//
char*  get_info_from_sessioninfo(char* mesg)
{
    if (mesg==NULL) return NULL;

    // Search in the Body
    char* pp = strstr(mesg, SESSION_INFO_KEY);
    if (pp==NULL) return NULL;
    pp = pp + strlen(SESSION_INFO_KEY);
    //
    char* pt = pp; 
    while (*pt!='&' && *pt!='\0') pt++;

    char bkup = *pt;
    *pt = '\0';
    char* ssninfo = dup_str(pp);
    *pt = bkup;
    //print_message("lms_sessioninfo = %s\n", ssninfo);

    return ssninfo;
}

