/* vi: set tabstop=4 nocindent noautoindent: */


#include "ltictr_relay.h"


#define  SESSION_ID_KEY      "session_id="

#define  LMS_SESSIONINFO_KEY "lms_sessioninfo="         // Instance id, LTI id
#define  LMS_SERVERURL_KEY   "lms_serverurl="           // Scheme:Server Name:Port Num
#define  LMS_SERVERPATH_KEY  "lms_serverpath="          // 
#define  LMS_RPCTOKEN_KEY    "lms_rpctoken="            //


char*    SessionInfo  = NULL;
char*    ServerURL    = NULL;
char*    ServerPath   = NULL;
char*    ServerToken  = NULL;



//
//    Server -> Client 
//
int   relay_to_client(int sock, SSL* ssl, tList* hdr, Buffer buf)
{
    int cc = 0;
    int http_com = HTTP_UNKNOWN_METHOD;;

    if (hdr!=NULL) http_com = hdr->ldat.id;;

    if (http_com>HTTP_UNKNOWN_METHOD) {   // HTTP Header
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
            snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", LMS_SESSIONINFO_KEY, SessionInfo);
            lp = add_protocol_header(lp, "set-cookie", cookie);
            snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", LMS_SERVERURL_KEY, ServerURL);
            lp = add_protocol_header(lp, "set-cookie", cookie);
            snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", LMS_SERVERPATH_KEY, ServerPath);
            lp = add_protocol_header(lp, "set-cookie", cookie);
            snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", LMS_RPCTOKEN_KEY, ServerToken);
            lp = add_protocol_header(lp, "set-cookie", cookie);
            freenull(SessionInfo);
            freenull(ServerURL);
            freenull(ServerPath);
            freenull(ServerToken);
        }
        //////////////////////////////////////////////////////////
        Buffer snd = rebuild_http_Buffer(hdr, &buf);
        cc = ssl_tcp_send(sock, ssl, (char*)snd.buf, snd.vldsz);
        free_Buffer(&snd);
        //////////////////////////////////////////////////////////
        return cc;
    }
    else {
        cc = ssl_tcp_send(sock, ssl, (char*)buf.buf, buf.vldsz);
    }


    //
    ///////////////////////////////////////////////////////////////////////
    // WebSocket
    static char host[] = "server";

    tJson* temp = NULL;
    tJson* json = NULL;
    json = ws_json_server(buf.buf, buf.vldsz);

    if (json!=NULL && json->next!=NULL) {

        tJson* sister = json->next;
        while (sister->esis!=NULL) sister = sister->esis;
        while (sister!=NULL) {
            //print_json(stderr, json);
            struct ws_info info;
            memset(&info, 0, sizeof(struct ws_info));
            //
            char* type = get_string_from_json(search_key_json(sister, "msg_type", TRUE, 1));
            if (type!=NULL && ex_strcmp("execute_reply", type)) { 
                info.status = get_string_from_json(find_double_key_json(sister, "content", "status"));
                if (info.status!=NULL) {
                    temp = find_double_key_json(sister, "header", "username");
                    info.username = get_string_from_json(temp);
                    if (info.username!=NULL) {
                        info.date = get_string_from_json(find_key_sister_json(temp, "date"));
                        temp = find_double_key_json(sister, "parent_header", "session");
                        info.session = get_string_from_json(temp);
                        if (info.session!=NULL) {
                            info.message = get_string_from_json(find_key_sister_json(temp, "msg_id"));
                            info.host    = host;
                            post_xmlrpc_server(&info);
                            freenull(info.message);
                            freenull(info.session);
                        }
                        freenull(info.date);
                        freenull(info.username);
                    }
                    freenull(info.status);
                }
                freenull(type);
            }
            sister = sister->ysis;
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
    if (sock<=0) return -1;

    int cc = 0;
    int http_com = HTTP_UNKNOWN_METHOD;;

    if (hdr!=NULL) http_com = hdr->ldat.id;

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
        char* sessionid = get_info_from_header(hdr, SESSION_ID_KEY);        // URL パラメータから session_id を得る
        if (sessionid!=NULL) {
            char* cookieinfo = get_info_from_cookie(hdr, LMS_SESSIONINFO_KEY);  // ヘッダから Cookie を得る
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
                //
                freenull(ServerURL);
                freenull(ServerPath);
                freenull(ServerToken);
                ServerURL   = get_info_from_cookie(hdr, LMS_SERVERURL_KEY);
                ServerPath  = get_info_from_cookie(hdr, LMS_SERVERPATH_KEY);
                ServerToken = get_info_from_cookie(hdr, LMS_RPCTOKEN_KEY);
                //
                post_xmlrpc_server(&info);
                //
                freenull(cookieinfo);
                freenull(sessionid);
            }
        }
    }
    //
    else if (http_com == HTTP_POST_METHOD) {
        if (SessionInfo==NULL) {
            if (ex_strcmp("oauth_version", (char*)buf.buf)) {
                SessionInfo = get_info_from_ltidata((char*)buf.buf, LMS_SESSIONINFO_KEY);  
                ServerURL   = get_info_from_ltidata((char*)buf.buf, LMS_SERVERURL_KEY);  
                ServerPath  = get_info_from_ltidata((char*)buf.buf, LMS_SERVERPATH_KEY);  
                ServerToken = get_info_from_ltidata((char*)buf.buf, LMS_RPCTOKEN_KEY);  
            }
            // 
            //if (SessionInfo==NULL && strstr(buf.buf,  LMS_SESSIONINFO_KEY) != NULL) {
            //    SessionInfo = get_info_from_ltidata((char*)buf.buf, LMS_SESSIONINFO_KEY);  
            //    ServerURL   = get_info_from_ltidata((char*)buf.buf, LMS_SERVERURL_KEY);  
            //    ServerPath  = get_info_from_ltidata((char*)buf.buf, LMS_SERVERPATH_KEY);  
            //    ServerToken = get_info_from_ltidata((char*)buf.buf, LMS_RPCTOKEN_KEY);  
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
    json = ws_json_client(buf.buf, buf.vldsz);

    if (json!=NULL && json->next!=NULL) {

        tJson* sister = json->next;
        while (sister->esis!=NULL) sister = sister->esis;
        while (sister!=NULL) {
            //
            struct ws_info info;
            memset(&info, 0, sizeof(struct ws_info));
            //
            temp = find_double_key_json(sister, "metadata", "cellId");
            info.cell_id = get_string_from_json(temp);
            if (info.cell_id!=NULL) {
                //info.tags = get_string_from_json(find_key_sister_json(temp, "tags"));
                temp = find_key_sister_json(temp, "tags");
                info.tags = get_string_from_json(temp);
                temp = find_double_key_json(sister, "header", "session");
                info.session = get_string_from_json(temp);
                if (info.session!=NULL) {
                    info.date     = get_string_from_json(find_key_sister_json(temp, "date"));
                    info.message  = get_string_from_json(find_key_sister_json(temp, "msg_id"));
                    info.host     = host;
                    post_xmlrpc_server(&info);
                    //
                    freenull(info.message);
                    freenull(info.date);
                    freenull(info.session);
                }
                freenull(info.tags);
                freenull(info.cell_id);
            }
            sister = sister->ysis;
        }
        //
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
char*  get_info_from_header(tList* hdr, char* key)
{
    if (hdr==NULL || key==NULL) return NULL;
    //
    tList* lp = search_key_tList(hdr, HDLIST_FIRST_LINE_KEY, 1);
    if (lp==NULL) return NULL;
    char* data = dup_str((char*)lp->ldat.val.buf);
    if (data==NULL) return NULL;

    char* pp = strstr(data, key); 
    if (pp==NULL) {
        free(data);
        return NULL;
    }
    pp = pp + strlen(key);

    char* pt = pp;
    while(*pt!=' ' && *pt!='&' && *pt!='%' && *pt!='\0') pt++;

    *pt = '\0';
    char* value = dup_str(pp);
    free(data);

    //print_message("get_info_from_header --> %s%s\n", key, value);
    return value;
}


//
// クライアント（Webブラウザ）のクッキーから コースIDとLTIのインスタンスID を取り出す．
// 要 free
//
char*  get_info_from_cookie(tList* hdr, char* key)
{
    if (hdr==NULL || key==NULL) return NULL;

    tList* lp = search_key_tList(hdr, "Cookie", 1);
    if (lp==NULL) return NULL;
    char* cke = dup_str((char*)lp->ldat.val.buf);
    if (cke==NULL) return NULL;

    char* pp = strstr(cke, key);
    if (pp==NULL) {
        free(cke);
        return NULL;
    }
    pp = pp + strlen(key);

    char* pt = pp; 
    while (*pt!=';' && *pt!='\0') pt++;

    *pt = '\0';
    char* value = dup_str(pp);
    free(cke);

    //print_message("get_info_from_cookie --> %s%s\n", key, value);
    return value;
}



//
// HTTPのPOST受信データ（mesg）のボディから コースIDとLTIのインスタンスID を取り出す．
// mesg は LTI コンシューマ（クライアント: Moodle）からのデータ．
// 要 free
//
char*  get_info_from_ltidata(char* mesg, char* key)
{
    if (mesg==NULL || key==NULL) return NULL;

    // Search in the Body
    char* pp = strstr(mesg, key);
    if (pp==NULL) return NULL;
    pp = pp + strlen(key);
    //
    char* pt = pp; 
    while (*pt!='&' && *pt!='\0') pt++;

    char bkup = *pt;
    *pt = '\0';
    char* value = dup_str(pp);
    *pt = bkup;

    //print_message("get_info_from_ltidata --> %s\n", key, value);
    return value;
}


