/*  
    Proxy Server for JupyterHub and LTIConrainerSpawner
        
                by Fumi.Iseki '22 01/16   BSD License.
*/

#include "ltictr_proxy.h"
#include "jbxl_state.h"
#include "http_tool.h"

#define  LTICTRPROXY_TIMEOUT   900       // 15m

#define  DEFAULT_PID_FILE   "/var/run/ltictr_proxy.pid"
#define  DEFAULT_CERT_FILE  "/etc/pki/tls/certs/server.pem"
#define  DEFAULT_KEY_FILE   "/etc/pki/tls/private/key.pem"


int    Nofd, Sofd;
int    Aofd, Pofd;
int    Cofd;

int    Log_Type;
pid_t  RootPID;
char*  PIDFile = NULL;

SSL*   Sssl = NULL;
SSL*   Cssl = NULL;
int    ClientSSL = OFF;     // サーバ側（自身はクライアント）とのSSL 接続
int    ServerSSL = OFF;     // クライアント側（自身はサーバ）とのSSL 接続

char*  ClientIPaddr  = NULL;
char*  ClientName    = NULL;
unsigned char*  ClientIPaddr_num  = NULL;



int main(int argc, char** argv)
{
    int  sport=0;
    socklen_t cdlen, pdlen;
    struct sockaddr  cl_addr, pl_addr;
    struct sigaction sa;
    struct passwd*  pw;

    //Buffer hostname;
    Buffer username;
    Buffer pidfile;
    Buffer certfile;
    Buffer keyfile;
    Buffer configfile;

    // for arguments
    //hostname   = init_Buffer();
    username   = init_Buffer();
    pidfile    = init_Buffer();
    certfile   = init_Buffer();
    keyfile    = init_Buffer();
    configfile = init_Buffer();

    for (int i=1; i<argc; i++) {
        if      (!strcmp(argv[i],"-p")) {if (i!=argc-1) sport = atoi(argv[i+1]);}
        //else if (!strcmp(argv[i],"-h")) {if (i!=argc-1) hostname = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-u")) {if (i!=argc-1) username = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-f")) {if (i!=argc-1) pidfile  = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-d")) DebugMode  = ON;
        //
        else if (!strcmp(argv[i],"-s")) ClientSSL  = ON;
        else if (!strcmp(argv[i],"-c")) ServerSSL  = ON;
        else if (!strcmp(argv[i],"--cert"))   {if (i!=argc-1) certfile   = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--key"))    {if (i!=argc-1) keyfile    = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--conf"))   {if (i!=argc-1) configfile = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--config")) {if (i!=argc-1) configfile = make_Buffer_bystr(argv[i+1]);}
        //
        else if (*argv[i]=='-') print_message("unknown argument: %s\n", argv[i]);
    }
    if (sport==0) {
        print_message("Usage... %s -p port [-s] [-c] [-u user] [-f pid_file] [-d] \n", argv[0]);
        print_message("                 [--conf config_file]  [--cert cert_file] [--key key_file]\n");
        exit(1);
    }

    //i = 0;
    //while(hostname.buf[i]!='\0' && hostname.buf[i]!=':') i++;
    //if (hostname.buf[i]==':') {
    //    cport = atoi((char*)&(hostname.buf[i+1]));
    //    hostname.buf[i] = '\0';
    //}
    //else cport = sport;

    if (pidfile.buf==NULL) copy_s2Buffer(DEFAULT_PID_FILE, &pidfile);
    PIDFile = (char*)pidfile.buf;

    // Config File
    tList* filelist = NULL;
    if (configfile.buf!=NULL) filelist = read_index_tList_file((char*)configfile.buf, '=');
    //
    if (certfile.buf==NULL) {
        char* cert = get_str_param_tList(filelist, "LTICTR_Server_Cert", DEFAULT_CERT_FILE);
        copy_s2Buffer(cert, &certfile);
        free(cert);
    }
    if (keyfile.buf==NULL) {
        char* key  = get_str_param_tList(filelist, "LTICTR_Private_Key", DEFAULT_KEY_FILE);
        copy_s2Buffer(key, &keyfile);
        free(key);
    }

    //
    // Initialization
    DEBUG_MODE print_message("Start initialization.\n");
    Log_Type = init_main(DebugMode, filelist);
    if (Log_Type<0) {
        print_message("Initialization failure.\n");
        exit(1);
    }
    DEBUG_MODE print_message("Initialization is finished.\n");

    // Signal handling
    sa.sa_handler = sig_term;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    #
    set_sigterm_child(NULL);            // Setting of child process is terminated

    // PID file
    RootPID = getpid();
    if (pidfile.buf!=NULL) {
        FILE*  fp;
        fp = fopen((char*)pidfile.buf, "w");
        if (fp!=NULL) {
            fprintf(fp, "%d", (int)RootPID);
            fclose(fp);
        }
    }

    // Change effective user
    if (username.buf!=NULL) {
        int err = -1;

        DEBUG_MODE print_message("Change effective user (%s)．\n", username.buf);
        if (isdigit(username.buf[0]) || username.buf[0]=='-') {
            err = seteuid(atoi((char*)username.buf));
        }
        else {
            pw = getpwnam((char*)username.buf);
            if (pw!=NULL) err = seteuid(pw->pw_uid);
        }
        if (err==-1) {
            syslog(Log_Type, "Cannot change effective user (%s): [%s]", username.buf, strerror(errno));
            DEBUG_MODE print_message("Cannot change effective user (%s).\n", username.buf);
        }
    }

    ////////////////////////////////////
    // Server API port
    DEBUG_MODE print_message("Port open for api connection.\n");
    Aofd = tcp_server_socket(-8001);    // non block socket
    if (Aofd<0) {
        syslog(Log_Type, "Open error of the port for api connection: [%s]", strerror(errno));
        print_message("Open error of the port for api connection.\n");
        exit(1);
    }

    // socket open for client
    DEBUG_MODE print_message("Server port open for client connection.\n");
    Nofd = tcp_server_socket(-sport);   // non block socket
    if (Nofd<0) {
        syslog(Log_Type, "Open error of the server port for client connection: [%s]", strerror(errno));
        print_message("Open error of the server port for client connection.\n");
        exit(1);
    }

    // for SSL/TLS
    SSL_CTX* server_ctx = NULL;
    SSL_CTX* client_ctx = NULL;
    if (ServerSSL==ON || ClientSSL==ON) {
        ssl_init();
        if (ServerSSL==ON) server_ctx = ssl_server_setup((char*)certfile.buf, (char*)keyfile.buf);
        if (ClientSSL==ON) client_ctx = ssl_client_setup(NULL);
    }

    // main loop
    DEBUG_MODE print_message("Start main loop.\n");
    cdlen = sizeof(cl_addr);
    pdlen = sizeof(pl_addr);
    //

    int port = 0;
    Sofd = Pofd = 0;

    Loop {
        if (Sofd==0) Sofd = accept(Nofd, &cl_addr, &cdlen);
        if (Pofd==0) Pofd = accept(Aofd, &pl_addr, &pdlen);
        //
        if (Sofd>0 || port>0) {
            if (fork()==0) receipt("127.0.0.1", port, cl_addr, server_ctx, client_ctx);
            close(Sofd);    // don't use socket_close() !
            Sofd = 0;
            port = 0;
        }
        //
        if (Pofd>0) {
            port = server_api(Pofd);
            if (port<=0) {
                close(Pofd);
                Pofd = 0;
                port = 0;
            }
        }
    }
    DEBUG_MODE print_message("Stop main loop.\n");

    //
    socket_close(Nofd);
    socket_close(Aofd);
    Sofd = Nofd = 0;
    Pofd = Aofd = 0;

    if (server_ctx!=NULL)  SSL_CTX_free(server_ctx);
    if (client_ctx!=NULL)  SSL_CTX_free(client_ctx);
    if (pidfile.buf!=NULL) remove((const char*)pidfile.buf);   

    //free_Buffer(&hostname);
    free_Buffer(&username);
    free_Buffer(&pidfile);
    free_Buffer(&certfile);
    free_Buffer(&keyfile);
    free_Buffer(&configfile);
    del_tList(&filelist);

    exit(0);
}




int  server_api(int sock)
{
    int   cc, len, hsz, csz;
    int   port;
    int   connect;
    int   tsecond = 30;

    static int lport = 4900;

    tList* lst = NULL;
    Buffer buf, cnt;

    // ヘッダの受信
    hsz = recv_http_header(sock, &lst, &len, NULL, &connect);
    if (hsz<=0 || len==0 || len==HTTP_HEADER_UNKNOWN_LEN) {
        send_http_error(sock, 400);
        del_tList(&lst);
        return -1;
    }

    // ヘッダ中に紛れ込んだコンテンツの取り出し
    buf = make_Buffer(RECVBUFSZ);
    cnt = search_protocol_header(lst, (char*)HDLIST_CONTENTS_KEY, 1);
    if (cnt.buf!=NULL) {
        csz = cnt.vldsz;
        hsz = hsz - csz;
        copy_Buffer(&cnt, &buf);
        free_Buffer(&cnt);
    }
    
    // コンテンツの受信
    if (connect) {
        if (len>0) {
            cc = recv_http_content(sock, &buf, len, tsecond, NULL, &connect);
        }
        else if (len==HTTP_HEADER_CHUNKED) {
            cc = recv_http_chunked(sock, &buf, tsecond, NULL, &connect);
        }
        else { //if (len==HTTP_HEADER_CLOSED_SESSION) {
            cc = recv_http_closed(sock, &buf, tsecond, NULL);
            connect = FALSE;
        }
    }
    //
    if (cc<0 || !connect) {
        send_http_error(sock, 400);
        del_tList(&lst);
        free_Buffer(&buf);
        return -1;
    }

    //
    port = -1;
    int com = get_http_header_method(lst);
    
    if      (com==HTTP_GET_METHOD) {

    }
    else if (com==HTTP_POST_METHOD) {
        port = get_notused_tcp_port(lport);
        if (port>0) lport = port + 1;
    }
    else if (com==HTTP_DELETE_METHOD) {
    }
    else {
        print_message("Not Supported Method : %d \n", com);
        print_tList(stderr, lst);
        print_message("%s\n", buf.buf);
        send_http_error(sock, 400);
    }

    free_Buffer(&buf);
    del_tList(&lst);
    //
    return port;
}




int  send_http_error(int sock, int err)
{
    tList* hdr = NULL;
    tList* lst = NULL;

    if (err==404) {
        lst = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 404 Not Found");
    }
    else {
        lst = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 400 Bad Request");
    }

    hdr = lst;
    hdr = add_tList_node_str(hdr, "Connection", "close");

    int cc = send_http_header(sock, lst, OFF);
    del_tList(&lst);

    return cc;
}




int  send_http_resp(int sock, int num, Buffer* buf)
{
    tList* hdr = NULL;
    tList* lst = NULL;

    if (num==200) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 200 OK");
        hdr = add_tList_node_str(hdr, "Content-Type", "application/json");
        hdr = add_tList_node_str(hdr, "Content-Length", "0");
    }
    else if (num==201) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 201 Created");
    }
    else if (num==204) {
        lst = hdr = add_tList_node_str(NULL, HDLIST_FIRST_LINE_KEY, "HTTP/1.1 204 Not Content");
    }

    hdr = add_tList_node_str(hdr, "Connection", "keep-alive");
    char* date = get_http_header_date(time(0));
    if (date!=NULL) {
        hdr = add_tList_node_str(hdr, "Date", date);
        free(date);
    }

    int cc = send_http_Buffer(sock, lst, buf);
    del_tList(&lst);

    return cc;
}




int  get_notused_tcp_port(int port)
{
    do {
        int sock = tcp_server_socket(port);
        if (sock>0) {
            socket_close(sock);
            port++;
        }
    } while (sock<0 && port<65536);

    if (port==65536) port = -1;

    return port;
}







//
void  receipt(char* hostname, int cport, struct sockaddr addr, SSL_CTX* server_ctx, SSL_CTX* client_ctx)
{
    int    cc, nd;
    fd_set mask;
    char msg[RECVBUFSZ];
    struct timeval timeout;

    DEBUG_MODE print_message("Start child process. (%d)\n", getpid());

    init_rand();

    struct sockaddr_in* addr_ptr = (struct sockaddr_in*)&addr;
    
    // モジュールの開始処理
    DEBUG_MODE print_message("モジュールの初期処理．\n");
    ClientIPaddr_num = get_ipaddr_num_ipv4(addr_ptr->sin_addr);
    ClientIPaddr     = get_ipaddr_ipv4(addr_ptr->sin_addr);
    ClientName       = get_hostname_bynum_ipv4(ClientIPaddr_num);
    syslog(Log_Type, "[%s] session start.\n", ClientIPaddr);

    if (!init_process(Sofd, ClientName)) {
        syslog(Log_Type, "module start error.");
        print_message("モジュールの初期処理の失敗．(%d)\n", getpid());
        exit(1);
    }
    DEBUG_MODE print_message("モジュールの初期処理完了．(%d)\n", getpid());

    Sssl = NULL;
    Cssl = NULL;

    // for Client SSL Connection
    if (ServerSSL==ON && server_ctx!=NULL) {
        Sssl = ssl_server_socket(Sofd, server_ctx);
        if (Sssl==NULL) {
            sleep(1);
            Sssl = ssl_server_socket(Sofd, server_ctx);
            if (Sssl==NULL) {
                print_message("クライアント用SSLソケットの作成失敗．(%d)\n", getpid());
                exit(1);
            }
        }
        DEBUG_MODE print_message("クライアント用SSLソケットのオープン．(%d)\n", getpid());
    }

    // for Server Connection
    Cofd = tcp_client_socket(hostname, cport);
    if (Cofd<0) {
        syslog(Log_Type, "tcp_client_socket() error: [%s]", strerror(errno));
        print_message("サーバへの接続に失敗．(%d)\n", getpid());
        exit(1);
    }
    if (ClientSSL==ON && client_ctx!=NULL && Sssl!=NULL) {
        Cssl = ssl_client_socket(Cofd, client_ctx, OFF);
        if (Cssl==NULL) {
            DEBUG_MODE print_message("サーバへのSSL接続の失敗．(%d)\n", getpid());
        }
    }
 
    int range = Max(Sofd, Cofd) + 1;

    //
    //do {
    timeout.tv_sec  = LTICTRPROXY_TIMEOUT;
    timeout.tv_usec = 0;
    FD_ZERO(&mask); 
    FD_SET(Sofd, &mask);
    FD_SET(Cofd, &mask);
    nd = select(range, &mask, NULL, NULL, &timeout);
    //} while (nd<0);

    DEBUG_MODE print_message("通信の中継処理開始．(%d)\n", getpid());
    while(nd>0 && (FD_ISSET(Cofd, &mask) || FD_ISSET(Sofd, &mask))) {
        // Client -> Server // fesrv はサーバ
        if (FD_ISSET(Sofd, &mask)) {
            memset(msg, 0, RECVBUFSZ);
            cc = ssl_tcp_recv(Sofd, Sssl, msg, RECVBUFSZ);      // Client から受信
            if (cc>0) {
                cc = fe_client(Sofd, Cofd, Sssl, Cssl, msg, cc);     // Server へ転送
                if (cc<=0) {
                    if (cc<0) syslog(Log_Type, "error occurred in fe_client().");
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

        // Server -> Client // fesrv はクライアント
        if (FD_ISSET(Cofd, &mask)) {
            memset(msg, 0, RECVBUFSZ);
            cc = ssl_tcp_recv(Cofd, Cssl, msg, RECVBUFSZ);      // Server から受信
            if (cc>0) {
                cc = fe_server(Cofd, Sofd, Cssl, Sssl, msg, cc);     // Client へ転送
                if (cc<=0) {
                    if (cc<0) syslog(Log_Type, "error occurred in fe_server().");
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
        timeout.tv_sec  = LTICTRPROXY_TIMEOUT;
        timeout.tv_usec = 0;
        FD_ZERO(&mask); 
        FD_SET(Sofd, &mask);
        FD_SET(Cofd, &mask);
        nd = select(range, &mask, NULL, NULL, &timeout);
        //} while (nd<0);
    }
    DEBUG_MODE print_message("End of communication. (%d)\n", getpid());

    ssl_close(Cssl);
    ssl_close(Sssl);
    socket_close(Cofd);
    Cssl = Sssl = NULL;
    Cofd = 0;

    syslog(Log_Type, "[%s] session end.", ClientIPaddr);

    // モジュールの終了処理
    DEBUG_MODE print_message("Termination of the child process. (%d)\n", getpid());
    if (!term_process(Sofd)) {
        syslog(Log_Type, "Error of termination of the child process.");
        print_message("Failure to terminate the child process. (%d)\n", getpid());
        exit(1);
    }

    socket_close(Sofd);
    DEBUG_MODE print_message("Termination of child process. (%d)\n", getpid());

    exit(0);
}
 


//
// Termination of program
//
void  sig_term(int signal)
{
    UNUSED(signal);

    pid_t pid = getpid();
    if (pid==RootPID) {
        if (term_main()) DEBUG_MODE print_message("Failure to terminate the program.(%d)\n", getpid());
    }

    if (Cofd>0) { close(Cofd); Cofd = 0;}
    if (Sofd>0) { close(Sofd); Sofd = 0;}
    if (Aofd>0) { close(Aofd); Aofd = 0;}
    if (Nofd>0) { close(Nofd); Nofd = 0;}
    if (Pofd>0) { close(Pofd); Pofd = 0;}
    if (PIDFile!=NULL) remove(PIDFile);

    closelog(); // close syslog 
    //exit(signal);
    exit(0);
}



//
// Termination of child process
//
void  sig_child(int signal)
{
    pid_t pid = 0;
    int ret;

    UNUSED(signal);
    //DEBUG_MODE print_message("SIG_CHILD: signal = %d\n", signal);

    do {
        pid = waitpid(-1, &ret, WNOHANG);
    } while(pid>0);
}



