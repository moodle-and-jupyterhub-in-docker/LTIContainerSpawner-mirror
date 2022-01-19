/*  
    Proxy Server for JupyterHub and LTIConrainerSpawner
        
                by Fumi.Iseki '22 01/16   BSD License.
*/

#include "ltictr_proxy.h"
#include "ltictr_api.h"
#include "ltictr_https.h"
#include "ltictr_nbws.h"

#include "jbxl_state.h"
#include "tjson.h"
#include "http_tool.h"

#define  LTICTRPROXY_TIMEOUT   900       // 15m

#define  DEFAULT_PID_FILE   "/var/run/ltictr_proxy.pid"
#define  DEFAULT_CERT_FILE  "/etc/pki/tls/certs/server.pem"
#define  DEFAULT_KEY_FILE   "/etc/pki/tls/private/key.pem"


int    Nofd, Sofd;
int    Mofd, Aofd;
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


tList*  ProxyList = NULL;


int main(int argc, char** argv)
{
    int  sport=0, cport;
    socklen_t cdlen, pdlen;
    struct sockaddr  cl_addr, pl_addr;
    struct sigaction sa;
    struct passwd*  pw;

    Buffer hostname;
    Buffer username;
    Buffer pidfile;
    Buffer certfile;
    Buffer keyfile;
    Buffer configfile;

    // for arguments
    hostname   = init_Buffer();
    username   = init_Buffer();
    pidfile    = init_Buffer();
    certfile   = init_Buffer();
    keyfile    = init_Buffer();
    configfile = init_Buffer();

    for (int i=1; i<argc; i++) {
        if      (!strcmp(argv[i],"-p")) {if (i!=argc-1) sport = atoi(argv[i+1]);}
        else if (!strcmp(argv[i],"-h")) {if (i!=argc-1) hostname = make_Buffer_bystr(argv[i+1]);}
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
    if (hostname.buf==NULL || sport==0) {
        print_message("Usage... %s -h host_name[:port] -p port [-s] [-c] [-i] [-u user] [-f pid_file] [-d] \n", argv[0]);
        print_message("                 [--conf config_file]  [--cert cert_file] [--key key_file]\n");
        exit(1);
    }
    //
    ProxyList = add_tList_node_anchor();

    int i = 0;
    while(hostname.buf[i]!='\0' && hostname.buf[i]!=':') i++;
    if (hostname.buf[i]==':') {
        cport = atoi((char*)&(hostname.buf[i+1]));
        hostname.buf[i] = '\0';
    }
    else cport = sport;

    if (pidfile.buf==NULL) copy_s2Buffer(DEFAULT_PID_FILE, &pidfile);
    PIDFile = (char*)pidfile.buf;

    //
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

    //
    // Signal handling
    sa.sa_handler = sig_term;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    #
    set_sigterm_child(NULL);            // Setting of child process is terminated

    //
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

    //
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

    //
    // Network
    // Server API port
    DEBUG_MODE print_message("Port open for api connection.\n");
    Mofd = tcp_server_socket(-8001);    // non block socket
    if (Mofd<0) {
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

    //
    // main loop
    DEBUG_MODE print_message("Start main loop.\n");
    cdlen = sizeof(cl_addr);
    pdlen = sizeof(pl_addr);
    //
    Sofd = Aofd = 0;

    Loop {
        if (Sofd==0) Sofd = accept(Nofd, &cl_addr, &cdlen);
        if (Aofd==0) Aofd = accept(Mofd, &pl_addr, &pdlen);
        //
        if (Sofd>0) {
            //receipt((char*)hostname.buf, cport, cl_addr, server_ctx, client_ctx);
            if (fork()==0) receipt((char*)hostname.buf, cport, server_ctx, client_ctx);
            close(Sofd);    // don't use socket_close() !
            Sofd = 0;
        }
        //
        if (Aofd>0) {
            api_process(Aofd, NULL, ProxyList);
        }
    }
    DEBUG_MODE print_message("Stop main loop.\n");

    //
    socket_close(Nofd);
    socket_close(Mofd);
    Sofd = Aofd = 0;

    if (server_ctx!=NULL)  SSL_CTX_free(server_ctx);
    if (client_ctx!=NULL)  SSL_CTX_free(client_ctx);
    if (pidfile.buf!=NULL) remove((char*)pidfile.buf);   

    free_Buffer(&hostname);
    free_Buffer(&username);
    free_Buffer(&pidfile);
    free_Buffer(&certfile);
    free_Buffer(&keyfile);
    free_Buffer(&configfile);
    del_tList(&filelist);

    del_all_tList(&ProxyList);

    exit(0);
}



//
//void  receipt(char* hostname, int cport, struct sockaddr addr, SSL_CTX* server_ctx, SSL_CTX* client_ctx)
void  receipt(char* hostname, int cport, SSL_CTX* server_ctx, SSL_CTX* client_ctx)
{
    int    cc, nd;
    fd_set mask;
    char msg[RECVBUFSZ];
    struct timeval timeout;

    tList* lst  = NULL;     // 受信ヘッダ
    Buffer buf;             // 受信ボディ
    char* uname = NULL;     // ex. bob
    char* path  = NULL;     // ex. /api/routes/user/bob

    Sssl = NULL;
    Cssl = NULL;

    // for Client SSL Connection
    if (ServerSSL==ON && server_ctx!=NULL) {
        Sssl = ssl_server_socket(Sofd, server_ctx);
        if (Sssl==NULL) {
            sleep(1);
            Sssl = ssl_server_socket(Sofd, server_ctx);
            if (Sssl==NULL) {
                print_message("クライアント用SSLサーバソケットの作成失敗．(%d)\n", getpid());
                exit(1);
            }
        }
        DEBUG_MODE print_message("クライアント用SSLサーバソケットのオープン．(%d)\n", getpid());
    }
    
    //
    int err = recv_https_request(Sofd, Sssl, &lst, &buf);
    //
    DEBUG_MODE {
        print_message("\n=== HTTP RECV ===\n");
        print_tList(stderr, lst);
        print_message("%s\n", buf.buf);
    }
    if (err>0) {
        free_Buffer(&buf);
        del_tList(&lst);
        send_https_error(Sofd, Sssl, err);
        print_message("クライアント用SSLサーバソケットで受信失敗．(%d)\n", getpid());
        exit(1);
    }

    get_http_header_method(lst, &path);    // get http command and path
    uname = get_username_api(path);                // get user name from path
    if (path!=NULL) free(path);
    if (uname==NULL) err = 400;
    if (err>0) {
        free_Buffer(&buf);
        del_tList(&lst);
        send_https_error(Sofd, Sssl, err);
        print_message("クライアント用SSLサーバソケットで処理失敗．(%d)\n", getpid());
        exit(1);
    }


    tList* pp = strncasecmp_tList(lst, uname, 0, 1);
    if (pp!=NULL) cport = (int)pp->ldat.id;


/*
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
*/

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

    send_https_Buffer(Cofd, Cssl, lst, &buf); 











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
    if (Mofd>0) { close(Mofd); Mofd = 0;}
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



