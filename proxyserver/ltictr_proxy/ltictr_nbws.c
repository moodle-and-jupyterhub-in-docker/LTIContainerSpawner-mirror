/* vi: set tabstop=4 nocindent noautoindent: */

/*  
    Front End Server Plugin for  Moodle & Jupyter Notebook Web Socket
        
                by Fumi.Iseki '22 01/16  v1.1.0  BSD License.
*/


#include "ltictr_proxy.h"
#include "tjson.h"
#include "https_tool.h"

int      Logtype;
tList*   Allow_IPaddr = NULL;

tList*   HTTP_Header  = NULL;
tList*   HTTP_Host    = NULL;
tList*   HTTP_Length  = NULL;
tList*   HTTP_Data    = NULL;

char*    SessionInfo  = NULL;

tList*   flist        = NULL;

#define  MOODLE_HOST_KEY     "Moodle_Host"
#define  MOODLE_PORT_KEY     "Moodle_Port"
#define  MOODLE_URL_KEY      "Moodle_URL"
#define  MOODLE_TOKEN_KEY    "Moodle_Token"
#define  MOODLE_SERVICE_KEY  "Moodle_Servide"
#define  MOODLE_DBANS_KEY    "Moodle_DBAns"
#define  MOODLE_TLS_KEY      "Moodle_TLS"
#define  MOODLE_HTTP_KEY     "Moodle_HTTP"

char*    Moodle_Host    = "localhost";
char*    Moodle_URL     = "/webservice/xmlrpc/server.php";
char*    Moodle_Token   = "";
char*    Moodle_Service = "mod_lticontainer_write_nbdata";
char*    Moodle_HTTP    = "1.1";
int      Moodle_Port    = 80;
int      Moodle_DBAns   = FALSE;
int      Moodle_TLS     = FALSE;



///////////////////////////////////////////////////////////////////////
// Web Socket Data

/*
void  print_buf(FILE* fp, unsigned char* buf, int size)
{
    int i;
    for(i=0; i<size; i++) {
        fprintf(fp, "%02x ", buf[i]);
        if (i%8==7) fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
    fflush(fp);
}
*/


tJson*  ws_json(Buffer* buf, char* mesg, int cc)
{
    tJson* json = NULL;

    unsigned char* pp = NULL;
    int pos = 0, mask, i;
    long int len = 0;
    
    //
    if (buf->state==1) {                // continue state
        cat_b2Buffer(mesg, buf, cc);
    }
    else {
        *buf = set_Buffer(mesg, cc);
    }
    pp = buf->buf;

    //unsigned char hd1 = pp[0] >> 4;
    //unsigned char hd2 = pp[0] & 0x0f;
    //if ((hd1==0x00 || hd1==0x08) && (hd2==0x00 || hd2==0x01 || hd2==0x02)) {

    // 非断片化のテキストのみサポート
    if (*pp==0x81) {
        mask = (int)(pp[1] >> 7);
        len  = (long int)(pp[1] & 0x7f);
        pos  = 2;
        if (len==126) {
            len = (long int)pp[2]*256 + (long int)pp[3];
            pos = 4;
        }
        else if (len==127) {
            len = (long int)pp[2];
            for(i=3; i<10; i++) {
                len = len*256 + (long int)pp[i];
                pos = 10;
            }
        }

        if (buf->vldsz>=pos+len+mask*4) {
            if (mask==1) {
                unsigned char* pm = &(pp[pos]);
                pos += 4;
                for (i=0; i<len; i++) {
                    pp[pos+i] = pp[pos+i] ^ pm[i%4];
                }
            }

            //print_message("%s\n",&(pp[pos]));
            if (pp[pos]=='{') {
                pp[pos+len] = '\0';
                json = json_parse((char*)&(pp[pos]), 0);
            }
            buf->state = 0;
        }
        else {
            buf->state = 1;
        }
    }
    else {
        buf->state = 0;
    }

    return json;
}



tJson*  ws_json_client(char* mesg, int cc)
{
    static Buffer buf;

    tJson* json = ws_json(&buf, mesg, cc);
    if (buf.state==0) {
        free_Buffer(&buf);
        buf.state = 0;
    }
    return json;
}

 
tJson*  ws_json_server(char* mesg, int cc)
{
    static Buffer buf;

    tJson* json = ws_json(&buf, mesg, cc);
    if (buf.state==0) {
        free_Buffer(&buf);
        buf.state = 0;
    }
    return json;
}



///////////////////////////////////////////////////////////////////////
// get information from HTTP

#define  SESSION_ID_KEY    "session_id="
#define  SESSION_INFO_KEY  "lms_sessioninfo="           // Instance id, LTI id


//
// URL パラメータから，存在するならば セッションID を取り出す．
// SESSION_ID_KEY はクライアント（Webブラウザ）からのリクエスト中のボディデータ中に設定されている値．
// 要 free
//
char*  get_sessionid_from_header(char* mesg)
{
    if (mesg==NULL) return NULL;
    //
    char* pl = get_line(mesg, 1);
    if (pl==NULL) return NULL;
    char* pp = strstr(pl, SESSION_ID_KEY);
    if (pp==NULL) return NULL;
    pp = pp + strlen(SESSION_ID_KEY);

    char* pt = pp;
    while(*pt!=' ' && *pt!='&' && *pt!='%' && *pt!='\0') pt++;

    *pt = '\0';
    char* sid = dup_str(pp);

    free(pl);
    
    return sid;
}


//
// HTTPのPOST受信データ（mesg）のボディから コースIDとLTIのインスタンスID を取り出す．
// mesg は LTI コンシューマ（クライアント: Moodle）からのデータ．
// 要 free
//
char*  get_info_from_sessioninfo(char* mesg)
{
    if (mesg==NULL) return NULL;

    char* pp = mesg;
    if (ex_strcmp("POST ", (const char*)mesg)){
        pp = strstr(mesg, "\r\n\r\n");    // Body
        if (pp==NULL) return NULL; 
    }
    //
    // Search in the Body
    pp = strstr(pp, SESSION_INFO_KEY);
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


//
// クライアント（Webブラウザ）のクッキーから コースIDとLTIのインスタンスID を取り出す．
// 要 free
//
char*  get_info_from_cookie(char* mesg)
{
    if (mesg==NULL) return NULL;

    char* pp = strstr(mesg, "Cookie:");
    if (pp==NULL) pp = strstr(mesg, "Cookie:");
    if (pp==NULL) return NULL;

    char* pl = get_line(pp, 1);
    if (pl==NULL) return NULL;
    pp = strstr(pl, SESSION_INFO_KEY);
    if (pp==NULL) return NULL;
    pp = pp + strlen(SESSION_INFO_KEY);

    char* pt = pp; 
    while (*pt!=';' && *pt!='\0') pt++;

    *pt = '\0';
    char* ssninfo = dup_str(pp);

    free(pl);

    return ssninfo;
}



///////////////////////////////////////////////////////////////////////
// tools

//
// JSON データのノード値の文字列を返す．
// "" または '' で囲まれている場合は，その内部のデータ（"", ''の中味）の返す．
// 要 free
//
char*  get_string_from_json(tJson* json)
{
    if (json==NULL) return NULL;

    char* str = NULL;
    char* pp  = (char*)json->ldat.val.buf;

    if (pp!=NULL && json->ldat.lv!=JSON_VALUE_ARRAY) {
        if (*pp=='\"' || *pp=='\'') {
            char* pt = (char*)&(json->ldat.val.buf[json->ldat.val.vldsz-1]);
            if (*pp==*pt) {
                pp++;
                char bkup = *pt;
                *pt = '\0';
                str = dup_str(pp);
                *pt = bkup;
            }
        }
    }
    else {
        str = dup_str(pp);
    }

    return str;
}



///////////////////////////////////////////////////////////////////////
// send data to Moodle Web Service

void  send_data_server()
{
    int sock = tcp_client_socket(Moodle_Host, Moodle_Port);
    if (sock<0) {
        print_message("ltictr_nbws: Connect to %s %d: ", Moodle_Host, Moodle_Port);
        jbxl_fprint_state(stderr, sock);
        return;
    }
    SSL_CTX* ctx = NULL;
    SSL* ssl     = NULL;

    if (Moodle_TLS) {
        ctx = ssl_client_setup(NULL);
        ssl = ssl_client_socket(sock, ctx, OFF);
    }

    DEBUG_MODE print_tList(stderr, HTTP_Header);
    send_https_header(sock, ssl, HTTP_Header, ON);
    if (Moodle_DBAns) {
        char ans[RECVBUFSZ];
        ssl_tcp_recv(sock, ssl, ans, RECVBUFSZ-1);
        print_message("%s\n", ans);
    }

    if (Moodle_TLS) {
        if (ssl!=NULL) ssl_close(ssl);
        SSL_CTX_free(ctx);
    }
    socket_close(sock);

    return;
}


void  post_xml_server(struct ws_info* info)
{
    tXML* xml = NULL;
    xml = xml_rpc_add_member(xml, "host",     info->host, "");
    xml = xml_rpc_add_member(xml, "inst_id",  info->inst_id, "");
    xml = xml_rpc_add_member(xml, "lti_id",   info->lti_id, "");
    xml = xml_rpc_add_member(xml, "session",  info->session, "");
    xml = xml_rpc_add_member(xml, "message",  info->message, "");
    xml = xml_rpc_add_member(xml, "status",   info->status, "");
    xml = xml_rpc_add_member(xml, "username", info->username, "");
    xml = xml_rpc_add_member(xml, "cell_id",  info->cell_id, "");
    xml = xml_rpc_add_member(xml, "tags",     info->tags, "");
    xml = xml_rpc_add_member(xml, "date",     info->date, "");
    xml = xml_rpc_end_member(xml);
    
    Buffer buf = xml_rpc_request_pack(Moodle_Service, xml);
    copy_i2Buffer((int)buf.vldsz, &(HTTP_Length->ldat.val));
    copy_Buffer(&buf, &(HTTP_Data->ldat.val));

    send_data_server();
    free_Buffer(&buf);
    del_xml(&xml);
}


void  init_xml_rpc_header(void)
{
    char url[LMESG];
    snprintf(url, LMESG-1, "POST %s?wstoken=%s HTTP/%s", Moodle_URL, Moodle_Token, Moodle_HTTP);

    tList* pp = NULL;
    pp = HTTP_Header = add_tList_node_bystr(pp, 0, 0, HDLIST_FIRST_LINE_KEY, url, NULL, 0);
    pp               = add_tList_node_bystr(pp, 0, 0, "Host", Moodle_Host, NULL, 0);
    pp               = add_tList_node_bystr(pp, 0, 0, "Content-Type", "text/html", NULL, 0);
    pp = HTTP_Length = add_tList_node_bystr(pp, 0, 0, "Content-Length", "", NULL, 0);
    pp               = add_tList_node_bystr(pp, 0, 0, "Connection", "close",  NULL, 0);
    pp               = add_tList_node_bystr(pp, 0, 0, HDLIST_END_KEY, "",  NULL, 0);
    pp = HTTP_Data   = add_tList_node_bystr(pp, 0, 0, HDLIST_CONTENTS_KEY,  "", NULL, 0);

    return;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// 全プロセスの共通初期化処理
//
int  init_main(int mode, tList* flist)
{
    //if (flist!=NULL) print_tTree(stderr, flist, "    ");

    //openlog("FEServer Jupyter WebScoket Plugin", LOG_PERROR|LOG_PID, LOG_AUTH);
    //Logtype = LOG_INFO;
    Logtype = NO_SYSLOG;
    DebugMode = mode;

    // 接続許可・禁止ファイルの読み込み
    //DEBUG_MODE print_message("アクセス制御ファイルの読み込み．\n");
    Allow_IPaddr = read_ipaddr_file(ALLOW_FILE);
    if (Allow_IPaddr!=NULL) {
        //syslog(Logtype, "Readed Access Allow tList.");
        //DEBUG_MODE print_message("アクセス許可ファイルの読み込み完了．\n");
        //DEBUG_MODE print_address_in_list(stderr, Allow_IPaddr);
    }
    else {
        syslog(Logtype, "Cannot read Access Contorol tList.");
        DEBUG_MODE print_message("アクセス制御ファイルの読み込み失敗．アクセス制御は行なわれません．\n");
    }

    // config file
    Moodle_Host    = get_str_param_tList (flist, MOODLE_HOST_KEY,    Moodle_Host);
    Moodle_URL     = get_str_param_tList (flist, MOODLE_URL_KEY ,    Moodle_URL);
    Moodle_Token   = get_str_param_tList (flist, MOODLE_TOKEN_KEY,   Moodle_Token);
    Moodle_Service = get_str_param_tList (flist, MOODLE_SERVICE_KEY, Moodle_Service);
    Moodle_HTTP    = get_str_param_tList (flist, MOODLE_HTTP_KEY,    Moodle_HTTP);
    Moodle_Port    = get_int_param_tList (flist, MOODLE_PORT_KEY,    Moodle_Port);
    Moodle_DBAns   = get_bool_param_tList(flist, MOODLE_DBANS_KEY,   Moodle_DBAns);
    Moodle_TLS     = get_bool_param_tList(flist, MOODLE_TLS_KEY,     Moodle_TLS);

    if (Moodle_Token[0]=='\0') {
        DEBUG_MODE print_message("Moodle Web Service に接続するためのトークンが指定されていません．\n");
    }

    init_xml_rpc_header();

    return  Logtype;
}



//
// 全プロセスの共通終了処理
//
int  term_main(void)
{
    del_tList(&HTTP_Header);

    free(Moodle_Host);
    free(Moodle_URL);
    free(Moodle_Token);
    free(Moodle_Service);
    free(Moodle_HTTP);

    return 0;
}



//
// プロセス毎の初期化処理
//
int  init_process(int dummy, char* client)
{
    UNUSED(dummy);

    //EBUG_MODE print_message("接続許可・禁止の確認．\n");
    if (Allow_IPaddr!=NULL) {
        unsigned char* ip_num = get_ipaddr_byname_num(client, AF_INET);
        char* client_ip = get_ipaddr_byname(client, AF_INET);

        if (is_host_in_list(Allow_IPaddr, ip_num, client)) {
            //DEBUG_MODE print_message("[%s] が許可ファイルにありました．\n", client_ip);
        }
        else {
            syslog(Logtype, "[%s] is access denied by AllowtList.", client_ip);
            DEBUG_MODE print_message("[%s] は許可ファイルに載っていません．\n", client_ip);
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

    del_all_tList(&Allow_IPaddr);

    if (SessionInfo!=NULL) free(SessionInfo);
    SessionInfo = NULL;

    return TRUE;
}



//
//    Server -> Client (cofd -> sofd) 
//
int   fe_server(int dummy1, int sofd, SSL* dummy2, SSL* ssl, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);
    Buffer shd = init_Buffer();

    int http_res = 0;

    //print_message("SERVER +++++++++++++++++++++++++++++++++++ %d\n", getpid());
    //print_message("%s\n",mesg);

    // add cookie
    // Session Info を lms_sessioninfo の値として cookie に追加
    if (SessionInfo!=NULL && ex_strcmp("HTTP/", (const char*)mesg)){
        http_res = 1;
        //
        Buffer buf = make_Buffer_bystr(mesg);
        tList* lhd = get_protocol_header_list(buf, ':', TRUE, TRUE);
        tList* chk = search_key_tList(lhd, HDLIST_CONTENTS_KEY, 1);
        //
        if (chk==NULL) {
            tList* cke = search_key_tList(lhd, "set-cookie", 1);
            if (cke==NULL) cke = search_key_tList(lhd, "Set-Cookie", 1);
            if (cke!=NULL) {
                char cookie[LMESG];
                snprintf(cookie, LMESG-1, "%s%s; HttpOnly; Path=/; Secure", SESSION_INFO_KEY, SessionInfo);
                cke  = add_protocol_header(cke, "set-cookie", cookie);
                shd  = restore_protocol_header(lhd, ": ", OFF, NULL);
                mesg = (char*)shd.buf;
                cc   = shd.vldsz;
                free(SessionInfo);
                SessionInfo = NULL;
            }
        }
        free_Buffer(&buf);
        del_tList(&lhd);
    }

    //////////////////////////////////////////////
    int ret = ssl_tcp_send(sofd, ssl, mesg, cc);
    //////////////////////////////////////////////

    if (shd.buf!=NULL) free_Buffer(&shd);


    //
    // Web Socket
    static char host[] = "server";

    tJson* temp = NULL;
    tJson* json = NULL;
    //if (*(unsigned char*)mesg==0x81) json = ws_json(mesg, cc);
    if (http_res==0) json = ws_json_server(mesg, cc);
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
                        post_xml_server(&info);
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



#define  CONTENT_LENGTH  "\r\ncontent-length: " 

//
//    Client -> Server (sofd -> cofd)
//
int   fe_client(int dummy1, int cofd, SSL* dummy2, SSL* ssl, char* mesg, int cc)
{
    UNUSED(dummy1);
    UNUSED(dummy2);

    //print_message("\nCLIENT +++++++++++++++++++++++++++++++++++ %d\n", getpid());
    //print_message("%s\n",mesg);

    //////////////////////////////////////////////
    cc = ssl_tcp_send(cofd, ssl, mesg, cc);
    //////////////////////////////////////////////

    static char ltictr_proxy[] = "ltictr_proxy";
    char num[10];

    static int content_length = 0;
    static Buffer recv_buffer;
    
    int http_com = 0;

    //
    // GET session_id と cookie の lms_sessionifo (course_id+%2C+lti_id) を関連付けて DB に登録．
    if (ex_strcmp("GET ", (const char*)mesg)) {
        http_com = 1;
        content_length = 0;
        recv_buffer = init_Buffer();
        //
        char* sessionid = get_sessionid_from_header(mesg); // URL パラメータから session_id を得る
        if (sessionid!=NULL) {
            char* ssninfo = get_info_from_cookie(mesg);
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
                info.host    = ltictr_proxy;
                info.inst_id = ssninfo;
                info.lti_id  = pt;
                info.session = sessionid;
                //
                post_xml_server(&info);
                //
                free(ssninfo);
                free(sessionid);
            }
        }
    }

    //
    else if (ex_strcmp("POST ", (const char*)mesg)) {
        http_com = 2;
        content_length = 0;
        recv_buffer = init_Buffer();
        char* pp = strstr(mesg, "\r\n\r\n");
        char* pt = strstrcase(mesg, CONTENT_LENGTH);

        // content-length の取り出し
        if (pp!=NULL && pt!=NULL && (int)(pp-pt)>0) {
            pt += strlen(CONTENT_LENGTH);
            char* pr = pt;
            while (*pr!=0x0d && *pr!=0x0a && *pr!=0x00) pr++;
            int l = (int)(pr - pt);
            if (l<=10) {
                memcpy(&num, pt, l);
                num[l] = 0x00;
                content_length = atoi(num);
            }
        } 
 
        // Body部 の取り出し
        if (pp!=NULL && content_length>0) {
            pp += strlen("\r\n\r\n");
            int l = cc - (int)(pp - (char*)mesg); 
            recv_buffer = set_Buffer(pp, l);
        }
    }
    else if (ex_strcmp("PUT ", (const char*)mesg)) {
        http_com = 3;
    }
    else if (ex_strcmp("DELETE ", (const char*)mesg)) {
        http_com = 4;
    }
    else if (ex_strcmp("PATCH ", (const char*)mesg)) {
        http_com = 5;
    }
    else if (content_length>0) {
        http_com = 2;
        cat_b2Buffer(mesg, &recv_buffer, cc);
    }

    // POST 受信完了
    if (content_length>0 && content_length==recv_buffer.vldsz) {
        mesg = (char*)recv_buffer.buf;
        //
        if (SessionInfo==NULL) {
            if (ex_strcmp("oauth_version", (const char*)mesg)) {
                SessionInfo = get_info_from_sessioninfo(mesg);  
            }
            // 
            if (SessionInfo==NULL && strstr(mesg,  SESSION_INFO_KEY) != NULL) {
                SessionInfo = get_info_from_sessioninfo(mesg);  
            }
        }
        content_length = 0;
        free_Buffer(&recv_buffer);
    }


    //
    // Web Socket
    static char host[]  = "client";

    tJson* temp = NULL;
    tJson* json = NULL;
    //if (*(unsigned char*)mesg==0x81) json = ws_json(mesg, cc);
    if (http_com==0) json = ws_json_client(mesg, cc);
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
                post_xml_server(&info);
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

