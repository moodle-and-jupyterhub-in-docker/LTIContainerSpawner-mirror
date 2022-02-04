/* vi: set tabstop=4 nocindent noautoindent: */


#include "ltictr_proxy.h"
#include "ltictr_signal.h"
#include "ltictr_relay.h"


#define  LTICTR_IDLETIME   900      // 15m
#define  LTICTR_TIMEOUT    3        // 3s


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
    init_xmlrpc_header();

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
            DEBUG_MODE print_message("[LTICTR_PROXY] Failure to create the server SSL socket. (%d)\n", getpid());
            sig_term(-1);
        }
        DEBUG_MODE print_message("[LTICTR_PROXY] Opened socket for SSL server. (%d) [%d]\n", getpid(), time(0));
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
    int len, connect = FALSE;
    //int http_com = HTTP_UNKNOWN_METHOD;
    int close_flag = OFF;
    //int webs_flag  = OFF;
    DEBUG_MODE print_message("[LTICTR_PROXY] Start Main Loop. (%d) [%d]\n", getpid(), time(0));

    //// Loop Start
    while(nd>0) {
        ///////////////////////////////////////////////////////////
        // Client -> Server
        if (FD_ISSET(ssock, &mask)) {
            //cc = recv_https_Buffer(ssock, sssl, &hdr, &buf, LTICTR_TIMEOUT, NULL, NULL);
            cc = recv_https_header(ssock, sssl, &hdr, &len, LTICTR_TIMEOUT, NULL, &connect);
            if (cc>0 && hdr!=NULL) {
                //
                //http_com = hdr->ldat.id;
                Buffer rcv = search_protocol_header(hdr, (char*)HDLIST_CONTENTS_KEY, 1);
                copy_Buffer(&rcv, &buf);
                free_Buffer(&rcv);

                lst = NULL;
                char* uname = NULL;
                //
                if (aport>0) {      // API Server を使用する
                    // ルーティング
                    uname = get_proxy_username(hdr);
                    if (uname!=NULL) {
                        // websocket
                        if (!strcmp(uname, "@")) {  // websock の相手を探す
                            lst = lproxy;
                            if (lst->ldat.id==TLIST_ANCHOR_NODE) lst = lst->next;
                                while (lst!=NULL) {
                                if (lst->ctrl==1) break;
                                lst = lst->next;
                            }
                        }
                        else {
                            lst = strncasecmp_tList(lproxy, uname, 0, 1);
                        }
                        // API Server へ問い合わせ
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

                // ヘッダ変更
                if (hdr->ldat.id>HTTP_UNKNOWN_METHOD) {
                    tList* con = search_key_tList(hdr, "Connection", 1);    // close, keep-alive, upgrade
                    if (con!=NULL) {
                        if (ex_strcmp("close", (char*)con->ldat.val.buf)) {
                        }
                        else if (strstrcase((char*)con->ldat.val.buf, "upgrade")==NULL) {
                            copy_s2Buffer("close", &con->ldat.val);
                        }
                    }
                    else {
                        tList* inst = find_protocol_end(hdr);
                        if (inst!=NULL) {
                            add_protocol_header(inst, "Connection", "close");
                        }
                    }
                }

                //DEBUG_MODE {
                    if (hdr->ldat.id>HTTP_UNKNOWN_METHOD) {
                        print_message("[LTICTR_PROXY] === HTTP RECV CLIENT === (%d) (%d) [%d]\n", ssock, getpid(), time(0));
                        print_protocol_header(hdr, OFF);
                        print_message("\n");
                    }
                    else {
                        if (lst->ctrl==1) {
                            print_message("[LTICTR_PROXY] === WEBSOCKET === (%d) (%d)\n", csock, getpid());
                        }
                    }
                //}
                //
                csock = get_proxy_socket(lst);
                cssl  = get_proxy_ssl(csock, client_ctx, lst);
                //
                cc    = relay_to_server(csock, cssl, hdr, buf, sproto);     // Server へ転送
                del_tList(&hdr);
                if (cc<=0) break;

                // Boby 転送
                if (connect) {
                    if (len>0) {
                        int sz = buf.vldsz;
                        while(sz<len) {
                            cc = ssl_tcp_recv_Buffer_wait(ssock, sssl, &buf, LTICTR_TIMEOUT);
                            if (cc>0) {
                                cc = relay_to_server(csock, cssl, NULL, buf, NULL);
                                sz += cc;
                            }
                            else break;
                        }
                    }
                    else if (len==HTTP_HEADER_CHUNKED) {
                        int sz = get_chunked_size((char*)buf.buf, &len);
                        while (sz>0) {
                            cc = ssl_tcp_recv_Buffer_wait(ssock, sssl, &buf, LTICTR_TIMEOUT);
                            if (cc>0) {
                                relay_to_server(csock, cssl, NULL, buf, NULL);
                                sz = get_chunked_size((char*)buf.buf, &len);
                            }
                            else break;
                        }
                    }
                    else if (len<0) {  //if (len==HTTP_HEADER_CLOSED_SESSION) {
                        //cc = ssl_tcp_recv_Buffer_wait(ssock, sssl, &buf, LTICTR_TIMEOUT);
                        //while(cc>0) {
                        //    relay_to_server(csock, cssl, NULL, buf, NULL);
                        //    cc = ssl_tcp_recv_Buffer_wait(ssock, sssl, &buf, LTICTR_TIMEOUT);
                        //}
                        //close_flag = ON;
                    }
                }
                if (cc<=0) break;
            }
            else {
                if (hdr!=NULL) del_tList(&hdr);
                break;      // cc==0
            }
        }

        //
        ///////////////////////////////////////////////////////////
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

        ///////////////////////////////////////////////////////////
        // Server -> Client // ltictr_proxy_erver はクライアント
        lst = lproxy;
        if (lst->ldat.id==TLIST_ANCHOR_NODE) lst = lst->next;
        while (lst!=NULL) {
            csock = lst->ldat.id;
            if (csock>0) {
                if (FD_ISSET(csock, &mask)) {
                    cssl = get_proxy_ssl(csock, client_ctx, lst);
                    //cc = recv_https_Buffer(csock, cssl, &hdr, &buf, LTICTR_TIMEOUT, NULL, NULL); 
                    cc = recv_https_header(csock, cssl, &hdr, &len, LTICTR_TIMEOUT, NULL, &connect);
                    if (cc>0 && hdr!=NULL) {
                        Buffer rcv = search_protocol_header(hdr, (char*)HDLIST_CONTENTS_KEY, 1);
                        copy_Buffer(&rcv, &buf);
                        free_Buffer(&rcv);
                        //
                        // ヘッダ変更
                        close_flag = OFF;
                        if (hdr->ldat.id>HTTP_UNKNOWN_METHOD) {
                            tList* con = search_key_tList(hdr, "Connection", 1);    // close, keep-alive, upgrade
                            if (con!=NULL) {
                                if (ex_strcmp("close", (char*)con->ldat.val.buf)) {
                                    close_flag = ON;
                                }
                                else if (strstrcase((char*)con->ldat.val.buf, "upgrade")==NULL) {
                                    copy_s2Buffer("close", &con->ldat.val);
                                    close_flag = ON;
                                }
                            }
                            else {
                                tList* inst = find_protocol_end(hdr);
                                if (inst!=NULL) {
                                    add_protocol_header(inst, "Connection", "close");
                                    close_flag = ON;
                                }
                            }
                        }
                        //
                        tList* wbs = search_key_tList(hdr, "Upgrade", 1);       // websocket
                        if (wbs!=NULL) {
                            if (ex_strcmp("websocket", (char*)wbs->ldat.val.buf)) {
                                lst->ctrl = 1;    
                            }
                        }
                        //

                        //DEBUG_MODE {
                            if (hdr->ldat.id>HTTP_UNKNOWN_METHOD) {
                                print_message("[LTICTR_PROXY] === HTTP RECV SERVER === (%d) (%d) [%d]\n", csock, getpid(), time(0));
                                print_protocol_header(hdr, OFF);
                                print_message("\n");
                            }
                            else {
                                if (lst->ctrl==1) {
                                    print_message("[LTICTR_PROXY] === WEBSOCKET === (%d) (%d)\n", csock, getpid());
                                }
                            }
                        //}

                        cc = relay_to_client(ssock, sssl, hdr, buf);     // Client へ転送

                        // Boby 転送
                        if (connect) {
                            if (len>0) {
                                int sz = buf.vldsz;
                                while(sz<len) {
                                    cc = ssl_tcp_recv_Buffer_wait(csock, cssl, &buf, LTICTR_TIMEOUT);
                                    if (cc>0) {
                                        relay_to_client(ssock, sssl, NULL, buf);
                                        sz += cc;
                                    }
                                    else break;
                                }
                            }
                            else if (len==HTTP_HEADER_CHUNKED) {
                                int sz = get_chunked_size((char*)buf.buf, &len);
                                while (sz>0) {
                                    cc = ssl_tcp_recv_Buffer_wait(csock, cssl, &buf, LTICTR_TIMEOUT);
                                    if (cc>0) {
                                        relay_to_client(ssock, sssl, NULL, buf);
                                        sz = get_chunked_size((char*)buf.buf, &len);
                                    }
                                    else break;
                                }
                            }
                            else if (len<0) {  //if (len==HTTP_HEADER_CLOSED_SESSION) {
                                //cc = ssl_tcp_recv_Buffer_wait(csock, cssl, &buf, 1);
                                //while(cc>0) {
                                //    relay_to_client(ssock, sssl, NULL, buf);
                                //    cc = ssl_tcp_recv_Buffer_wait(csock, cssl, &buf, 1);
                                //}
                                //close_flag = ON;
                            }
                        }

                        if (cc<=0 || close_flag==ON) {
                            ssl_close(cssl);
                            close(csock);
                            lst->ldat.id = 0;
                            if (lst->ldat.ptr!=NULL) lst->ldat.ptr = NULL;
                            lst->ldat.sz = 0;
                            lst->ctrl    = 0;
                            //break;
                        }
                    }
                    else {
                        ssl_close(cssl);
                        close(csock);
                        lst->ldat.id = 0;
                        if (lst->ldat.ptr!=NULL) lst->ldat.ptr = NULL;
                        lst->ldat.sz = 0;
                        lst->ctrl    = 0;
                        //break;      // cc==0
                    }
                    if (hdr!=NULL) del_tList(&hdr);
                }
            }
            lst = lst->next;
        }

        //
        ///////////////////////////////////////////////////////////
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
    //// Loop End

    DEBUG_MODE print_message("[LTICTR_PROXY] Stop  Main Loop. (%d) [%d]\n", getpid(), time(0));

    //
    ssl_close(sssl);
    close(ssock);

    lst = lproxy;
    if (lst->ldat.id==TLIST_ANCHOR_NODE) lst = lst->next;
    while (lst!=NULL) {
        csock = lst->ldat.id;
        cssl  = NULL;
        if (csock>0) {
            if (lst->ldat.ptr!=NULL) {
                cssl = (SSL*)lst->ldat.ptr;
                ssl_close(cssl);
                lst->ldat.ptr = NULL;
            }
            lst->ldat.sz = 0;
            lst->ldat.id = 0;
            lst->ctrl    = 0;
            close(csock);
        }
        lst = lst->next;
    }
    if (api_ctx!=NULL) SSL_CTX_free(api_ctx);

    free(sproto);

    DEBUG_MODE print_message("[LTICTR_PROXY] Termination of Process. (%d) [%d]\n", getpid(), time(0));
    _exit(0);
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int  get_proxy_socket(tList* lst)
{
    int sock = 0;
    if (lst==NULL) return -1;
    //
    sock = (int)lst->ldat.id;
    if (sock<=0) {
        //DEBUG_MODE print_message("[LTICTR_PROXY] Socket for %s is invalid. Reopen socket. (%d)\n", (char*)lst->ldat.key.buf, sock);
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
                DEBUG_MODE print_message("[LTICTR_PROXY] Failure to open the client SSL port. (%d)\n", getpid());
                sig_term(-1);
            }
        }
    }
    return ssl;
}



#define  LTICTR_HTTPS_HUB   "/hub/"
#define  LTICTR_HTTPS_USER  "/user/"


char*  get_proxy_username(tList* hdr)
{
    if (hdr==NULL) return NULL;

    char* unm = NULL;
    int http_com = hdr->ldat.id;

    if (http_com>HTTP_UNKNOWN_METHOD) {
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
        if (ex_strcmp(LTICTR_HTTPS_USER, path)) {
            char* pp = path + strlen(LTICTR_HTTPS_USER);
            char* pt = pp;
            while (*pt!='/' && *pt!='\0') pt++;
            *pt = '\0';
            unm = dup_str(pp);
        }
        free(path);
    }
    // websocket or unknown binary communication
    else {  
        unm = dup_str("@");
    }

    if (unm==NULL) unm = dup_str("/");

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
    close(sofd);

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


