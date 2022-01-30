/* vi: set tabstop=4 nocindent noautoindent: */


#include "ltictr_relay.h"



#define  SESSION_ID_KEY    "session_id="
#define  SESSION_INFO_KEY  "lms_sessioninfo="           // Instance id, LTI id


char*    SessionInfo  = NULL;



//
//    Server -> Client 
//
int   relay_to_client(int sock, SSL* ssl, tList* hdr, Buffer buf)
{
    if (hdr==NULL) return -1;

    int cc = 0;
    int http_com = hdr->ldat.id;

    if (http_com > HTTP_UNKNOWN_METHOD) {   // HTTP
        char*  resp = NULL;
        tList* lp = search_key_tList(hdr, HDLIST_FIRST_LINE_KEY, 1);
        if (lp!=NULL) resp = (char*)lp->ldat.val.buf;

        // add cookie
        // Session Info を lms_sessioninfo の値として cookie に追加
        if (SessionInfo!=NULL && resp!=NULL && ex_strcmp("HTTP/", resp)) {
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
        cc = ssl_tcp_send(sock, ssl, (char*)snd.buf, snd.vldsz);
        free_Buffer(&snd);
        //////////////////////////////////////////////////////////
    }
    else {
        cc = ssl_tcp_send(sock, ssl, (char*)buf.buf, buf.vldsz);
    }
    //
    if (http_com>HTTP_UNKNOWN_METHOD) return cc;


    //
    ///////////////////////////////////////////////////////////////////////
    // WebSocket
    static char host[] = "server";

    tJson* temp = NULL;
    tJson* json = NULL;
    json = ws_json_server((char*)buf.buf, buf.vldsz);

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

    return cc;
}



//
//    Client -> Server 
//
int   relay_to_server(int sock, SSL* ssl, tList* hdr, Buffer buf, char* proto)
{
    if (sock<=0 || hdr==NULL) return -1;

    int cc = 0;
    int http_com = hdr->ldat.id;

    if (http_com>HTTP_UNKNOWN_METHOD) {
        tList* ph = search_key_tList(hdr, "Host", 1);
        add_protocol_header(ph, "X-Forwarded-Proto", proto);
        //
        Buffer snd = rebuild_http_Buffer(hdr, &buf);
        cc = ssl_tcp_send(sock, ssl, (char*)snd.buf, snd.vldsz);
        free_Buffer(&snd);
    }
    else {
        cc = ssl_tcp_send(sock, ssl, (char*)buf.buf, buf.vldsz);
    }


    static char ltictr[] = "ltictr";
    //
    // GET session_id と cookie の lms_sessionifo (course_id+%2C+lti_id) を関連付けて XMLRPC で送る．
    if (http_com == HTTP_GET_METHOD) {
        char* sessionid = get_sessionid_from_header(hdr);   // URL パラメータから session_id を得る
        if (sessionid!=NULL) {
            char* cookieinfo = get_info_from_cookie(hdr);          // ヘッダから Cookie を得る
            if (cookieinfo!=NULL) {
                struct ws_info info;
                memset(&info, 0, sizeof(struct ws_info));
                //
                char* pt = cookieinfo;
                while (*pt!='%' && *pt!='\0') pt++;
                if (*pt=='%') {
                    *pt = '\0';
                    pt = pt + 3;
                }
                info.host    = ltictr;
                info.inst_id = cookieinfo;
                info.lti_id  = pt;
                info.session = sessionid;
                post_xmlrpc_server(&info);
                //
                free(cookieinfo);
                free(sessionid);
            }
        }
    }
    //
    else if (http_com == HTTP_POST_METHOD) {
        if (SessionInfo==NULL) {
            if (ex_strcmp("oauth_version", (char*)buf.buf)) {
                SessionInfo = get_info_from_ltidata((char*)buf.buf);  
            }
            // 
            //if (SessionInfo==NULL && strstr(buf.buf,  SESSION_INFO_KEY) != NULL) {
            //    SessionInfo = get_info_from_ltidata((char*)buf.buf);  
            //}
        }
    }
    //
    if (http_com>HTTP_UNKNOWN_METHOD) return cc;


    //
    ///////////////////////////////////////////////////////////////////////
    // WebSocket
    static char host[]  = "client";

    tJson* temp = NULL;
    tJson* json = NULL;
    //if (*(unsigned char*)mesg==0x81) json = ws_json(mesg, cc);
    json = ws_json_client((char*)buf.buf, buf.vldsz);

    if (json!=NULL) {
        struct ws_info info;
        memset(&info, 0, sizeof(struct ws_info));
        //
        temp = find_double_key_json(json, "metadata", "cellId");
        info.cell_id = get_string_from_json(temp);
        if (info.cell_id!=NULL) {
            //info.tags = get_string_from_json(find_key_sister_json(temp, "tags"));
            temp = find_key_sister_json(temp, "tags");
            info.tags = get_string_from_json(temp);
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



///////////////////////////////////////////////////////////////////////
// get information from HTTP

//
// 存在するならば セッションID を取り出す．
// SESSION_ID_KEY はクライアント（Webブラウザ）からのリクエストの ヘッダ中に設定されている値．
// 要 free
//
char*  get_sessionid_from_header(tList* hdr)
{
    if (hdr==NULL) return NULL;
    //
    tList* lp = search_key_tList(hdr, HDLIST_FIRST_LINE_KEY, 1);
    if (lp==NULL)  return NULL;
    char* data = dup_str((char*)lp->ldat.val.buf);
    if (data==NULL) return NULL;

    char* pp = strstr(data, SESSION_ID_KEY); 
    if (pp==NULL) {
        free(data);
        return NULL;
    }
    pp = pp + strlen(SESSION_ID_KEY);

    char* pt = pp;
    while(*pt!=' ' && *pt!='&' && *pt!='%' && *pt!='\0') pt++;

    *pt = '\0';
    char* sid = dup_str(pp);

    free(data);
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
    char* cookieinfo = dup_str(pp);

    free(cke);
    return cookieinfo;
}



//
// HTTPのPOST受信データ（mesg）のボディから コースIDとLTIのインスタンスID を取り出す．
// mesg は LTI コンシューマ（クライアント: Moodle）からのデータ．
// 要 free
//
char*  get_info_from_ltidata(char* mesg)
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

