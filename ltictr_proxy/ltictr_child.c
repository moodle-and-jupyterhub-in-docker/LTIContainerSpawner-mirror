/* vi: set tabstop=4 nocindent noautoindent: */


#include "ltictr_child.h"
#include "ltictr_signal.h"



#define  LTICTR_TIMEOUT    900       // 15m

#define  SESSION_ID_KEY    "session_id="
#define  SESSION_INFO_KEY  "lms_sessioninfo="           // Instance id, LTI id


char*    SessionInfo  = NULL;

extern tList* AllowList;
extern int    ClientSSL;


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



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// プロセス毎の初期化処理
//
int  init_process(int dummy, char* client)
{
    UNUSED(dummy);

    //EBUG_MODE print_message("接続許可・禁止の確認．\n");
    if (AllowList!=NULL) {
        unsigned char* ip_num = get_ipaddr_byname_num(client, AF_INET);
        char* client_ip = get_ipaddr_byname(client, AF_INET);

        if (is_host_in_list(AllowList, ip_num, client)) {
            //DEBUG_MODE print_message("[%s] が許可ファイルにありました．\n", client_ip);
        }
        else {
            //syslog(Logtype, "[%s] is access denied by AllowtList.", client_ip);
            DEBUG_MODE print_message("[%s] is not in the allow list. \n", client_ip);
            return FALSE;
        }
    }

    //
    return TRUE;
}



//
// プロセス毎の終了処理
//
int  term_process(int dummy)
{
    UNUSED(dummy);

    del_all_tList(&AllowList);

    if (SessionInfo!=NULL) free(SessionInfo);
    SessionInfo = NULL;

    return TRUE;
}



//
//    Server -> Client 
//
int   send_client(int sock, SSL* ssl, tList* hdr, Buffer buf)
{
    if (hdr==NULL) return -1;

    //int http_res = OFF;

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

    //////////////////////////////////////////////////////////
    Buffer snd = rebuild_http_Buffer(hdr, &buf);
    int ret = ssl_tcp_send(sock, ssl, (char*)buf.buf, buf.vldsz);
    free_Buffer(&snd);
    //////////////////////////////////////////////////////////


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

    return ret;
}



//
//    Client -> Server 
//
int   send_server(int sock, SSL* ssl, tList* hdr, Buffer buf, int http_com, char* proto)
{
    if (hdr==NULL) return -1;

    tList* ph = search_key_tList(hdr, "Host", 1);
    add_protocol_header(ph, "X-Forwarded-Proto", proto);

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

    //
    return cc;
}








//
void  receipt_child(char* hostname, int cport, int ssock, SSL_CTX* client_ctx, SSL_CTX* server_ctx, tList* lproxy)
{
    int    cc, nd;
    fd_set mask;
    struct timeval timeout;

    Buffer buf   = init_Buffer();   // 受信ボディ
    tList* hdr   = NULL;            // 受信ヘッダ

    //char* cproto = NULL;
    char* sproto = NULL;

    SSL* sssl = NULL;
    SSL* cssl = NULL;
    
    // Client SSL connection for data recieve
    if (server_ctx!=NULL) {
        sproto = dup_str("https");
        sssl = ssl_server_socket(ssock, server_ctx);
        if (sssl==NULL) {
            free(sproto);
            print_message("Failure to create the client socket. (%d)\n", getpid());
            sig_term(-1);
        }
        DEBUG_MODE print_message("Opened socket for SSL server. (%d)\n", getpid());
    }
    else sproto = dup_str("http");
    
    // データ受信
    buf = make_Buffer(RECVBUFSZ);
    int ret = recv_https_Buffer(ssock, sssl, &hdr, &buf, 0, NULL, NULL);
    if (ret<=0) {           // 0 は正常切断
        free(sproto);
        del_tList(&hdr);
        free_Buffer(&buf);
        if (ret<0) {
            send_https_error(ssock, sssl, 400);
            DEBUG_MODE print_message("Failure to recieve data from Web beowser. (%d)\n", getpid());
            sig_term(-1);
        }
        else {
            sig_term(0);
        }
    }

    DEBUG_MODE {
        print_message("\n=== HTTP RECV ===\n");
        print_tList(stderr, hdr);
        print_message("%s\n", (char*)buf.buf);
    }

    int http_com;
    int csock = get_proxy_socket(hdr, &http_com, hostname, cport, ClientSSL, lproxy);
    if (csock<=0) {
        free(sproto);
        del_tList(&hdr);
        free_Buffer(&buf);
        send_https_error(ssock, sssl, -ret);
        DEBUG_MODE print_message("Failure to get proxy client socket. (%d)\n", getpid());
        sig_term(-1);
    }
    if (ex_strcmp("https:", (char*)hdr->ldat.val.buf)) {
        ClientSSL = ON;
        //cproto = dup_str("https");
        cssl = ssl_client_socket(csock, client_ctx, OFF);
        if (cssl==NULL) {
            DEBUG_MODE print_message("Failure to connect to server SSL port. (%d)\n", getpid());
            sig_term(-1);
        }
    }
    else {
        ClientSSL = OFF;
        //cproto = dup_str("http");
    }
    
    // データ送信
    cc = send_server(csock, cssl, hdr, buf, http_com, sproto); 
    if (cc<=0) {
        //if (cc<0) syslog(Log_Type, "error occurred in fe_client().");
    }
    del_tList(&hdr);
    //free_Buffer(&buf);



    int range = Max(ssock, csock) + 1;
    //
    //do {
    timeout.tv_sec  = LTICTR_TIMEOUT;
    timeout.tv_usec = 0;
    FD_ZERO(&mask); 
    FD_SET(ssock,  &mask);
    FD_SET(csock, &mask);
    nd = select(range, &mask, NULL, NULL, &timeout);
    //} while (nd<0);

    DEBUG_MODE print_message("通信の中継処理開始．(%d)\n", getpid());
    while(nd>0 && (FD_ISSET(csock, &mask) || FD_ISSET(ssock, &mask))) {
        // Client -> Server // ltictr_proxy はサーバ
        if (FD_ISSET(ssock, &mask)) {
            cc = recv_https_Buffer(ssock, sssl, &hdr, &buf, 0, NULL, NULL);
            if (cc>0) {
                http_com = get_http_header_method(hdr, NULL);
                cc = send_server(csock, cssl, hdr, buf, http_com, sproto);     // Server へ転送
                if (cc<=0) {
                    //if (cc<0) syslog(Log_Type, "error occurred in fe_client().");
                    break;
                }
            }
            else {
                if (cc<0) {
                    print_message("ltictr_proxy: C->S: ");
                    jbxl_fprint_state(stderr, cc);
                }
                break;      // cc==0
            }
        }

        // Server -> Client // ltictr_proxy はクライアント
        if (FD_ISSET(csock, &mask)) {
            cc = recv_https_Buffer(csock, cssl, &hdr, &buf, 0, NULL, NULL); 
            if (cc>0) {
                cc = send_client(ssock, sssl, hdr, buf);     // Client へ転送
                if (cc<=0) {
                    //if (cc<0) syslog(Log_Type, "error occurred in fe_server().");
                    break;
                }
            }
            else {
                if (cc<0) {
                    print_message("ltictr_proxy: S->C: ");
                    jbxl_fprint_state(stderr, cc);
                }
                break;      // cc==0
            }
        }
        // dp {
        timeout.tv_sec  = LTICTR_TIMEOUT;
        timeout.tv_usec = 0;
        FD_ZERO(&mask); 
        FD_SET(ssock,  &mask);
        FD_SET(csock, &mask);
        nd = select(range, &mask, NULL, NULL, &timeout);
        //} while (nd<0);
    }
    DEBUG_MODE print_message("End of communication. (%d)\n", getpid());

    ssl_close(cssl);
    ssl_close(sssl);
    socket_close(csock);
    socket_close(ssock);
    free(sproto);
    //free(cproto);

    //syslog(Log_Type, "[%s] session end.", ClientIPaddr);

    DEBUG_MODE print_message("Termination of child process. (%d)\n", getpid());

    exit(0);
}
 




int  get_proxy_socket(tList* hdr, int* http_com, char* hostname, int cport, int use_ssl, tList* lproxy)
{
    char* path  = NULL;                                 // ex. /hub/lti/launch
    int   com   = get_http_header_method(hdr, &path);                // get http command and path
    char* uname = get_https_username(path);            // get user name from path
    if (http_com!=NULL) *http_com = com;
    if (path!=NULL)  free(path);
    if (uname==NULL) return -400;

    // Proxy 処理
    int csock = 0;
    tList* lp = strncasecmp_tList(lproxy, uname, 0, 1);
    if (lp!=NULL) {
        csock = (int)lp->ldat.id;
        if (csock<=0) {
            DEBUG_MODE print_message("socket for %s is invalid. Reopen socket.\n", uname);
            //
            char* hp = (char*)lp->ldat.val.buf + lp->ldat.val.vldsz;
            while(*hp!='/') hp--;
            char* hname = dup_str(hp + 1);
            csock = tcp_client_socket(hname, lp->ldat.lv);
            free(hname);
            //
            if (csock>0) {
                char* lasttime = get_local_timestamp(time(0), "%Y-%b-%dT%H:%M:%SZ");
                lp->ldat.id = csock;
                if (lp->ldat.ptr!=NULL) free(lp->ldat.ptr);
                lp->ldat.ptr = lasttime;
                lp->ldat.sz  = strlen(lasttime) + 1;
            }
            else csock = -500;
        }
    }
    // リストに無い
    else {
        if (hostname!=NULL) {
            csock = tcp_client_socket(hostname, cport);
            if (csock>0) {
                char* lasttime = get_local_timestamp(time(0), "%Y-%b-%dT%H:%M:%SZ");
                Buffer hnm;
                if (use_ssl==ON) hnm = make_Buffer_str("https://");
                else             hnm = make_Buffer_str("http://");
                cat_s2Buffer(hostname, &hnm);
                //
                tList* end = find_tList_end(lproxy);
                add_tList_node_bystr(end, csock, cport, uname, (char*)hnm.buf, lasttime, strlen(lasttime)+1);
                free(lasttime);
                free_Buffer(&hnm);
            }
            else csock = -500;
        }
        else csock = -500;
    }

    free(uname);
    return csock;
}
