/*  
    Proxy Server for JupyterHub and LTIConrainerSpawner
        
                by Fumi.Iseki '22 01/16   BSD License.
*/

#include "ltictr_proxy.h"
#include "jbxl_state.h"

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
    int  i, sport=0, cport;
    socklen_t cdlen;
    //struct sockaddr_in cl_addr;
    struct sockaddr  cl_addr;
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

    for (i=1; i<argc; i++) {
        if      (!strcmp(argv[i],"-p")) {if (i!=argc-1) sport = atoi(argv[i+1]);}
        else if (!strcmp(argv[i],"-h")) {if (i!=argc-1) hostname   = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-u")) {if (i!=argc-1) username   = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-f")) {if (i!=argc-1) pidfile    = make_Buffer_bystr(argv[i+1]);}
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
        print_message("Usage... %s -h host_name[:port] -p port [-s] [-c] [-u user] [-f pid_file] [-d] \n", argv[0]);
        print_message("                 [--conf config_file]  [--cert cert_file] [--key key_file]\n");
        exit(1);
    }

    i = 0;
    while(hostname.buf[i]!='\0' && hostname.buf[i]!=':') i++;
    if (hostname.buf[i]==':') {
        cport = atoi((char*)&(hostname.buf[i+1]));
        hostname.buf[i] = '\0';
    }
    else cport = sport;

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

    // Server connection test
    //DEBUG_MODE print_message("Start server response confirmation.\n");
    //Cofd = tcp_client_socket((char*)hostname.buf, cport);
    //if (Cofd<0) {
    //    syslog(Log_Type, "Server port inaccessible: [%s]", strerror(errno));
    //    DEBUG_MODE print_message("Server port inaccessible.\n");
    //    //exit(1);
    //}
    //socket_close(Cofd);
    //DEBUG_MODE print_message("Server response confirmation is finished.\n");

    // Signal handling
    sa.sa_handler = sig_term;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    #
    set_sigterm_child(NULL);            // Setting of child process is terminated

    // Server API port
    DEBUG_MODE print_message("Port open for api connection.\n");
    Aofd = tcp_server_socket(8001);
    if (Aofd<0) {
        syslog(Log_Type, "Open error of the port for api connection: [%s]", strerror(errno));
        print_message("Open error of the port for api connection.\n");
        exit(1);
    }

    // socket open for client
    DEBUG_MODE print_message("Port open for client connection.\n");
    Nofd = tcp_server_socket(sport);
    if (Nofd<0) {
        syslog(Log_Type, "Open error of the port for client connection: [%s]", strerror(errno));
        print_message("Open error of the port for client connection.\n");
        exit(1);
    }
    cdlen = sizeof(cl_addr);

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
    Loop {
        Sofd = accept_intr(Nofd, &cl_addr, &cdlen);
        if (Sofd<0) {
            syslog(Log_Type, "Connection failure from client: [%s]", strerror(errno));
            print_message("Connection failure from client.\n");
            //exit(1);
        }
        else {
            if (fork()==0) receipt((char*)hostname.buf, cport, cl_addr, server_ctx, client_ctx);
            socket_close(Sofd);
        }
    }
    DEBUG_MODE print_message("Stop main loop.\n");

    //
    socket_close(Nofd);
    Sofd = Nofd = 0;

    if (server_ctx!=NULL)  SSL_CTX_free(server_ctx);
    if (client_ctx!=NULL)  SSL_CTX_free(client_ctx);
    if (pidfile.buf!=NULL) remove((const char*)pidfile.buf);   

    free_Buffer(&hostname);
    free_Buffer(&username);
    free_Buffer(&pidfile);
    free_Buffer(&certfile);
    free_Buffer(&keyfile);
    free_Buffer(&configfile);
    del_tList(&filelist);

    exit(0);
}


//
void  receipt(char* hostname, int cport, struct sockaddr addr, SSL_CTX* server_ctx, SSL_CTX* client_ctx)
{
    int    cc, nd;
    fd_set mask;
    char msg[RECVBUFSZ];
    struct timeval timeout;

    DEBUG_MODE print_message("子プロセスの開始．（%d）\n", getpid());

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
    DEBUG_MODE print_message("通信の中継処理終了．(%d)\n", getpid());

    ssl_close(Cssl);
    ssl_close(Sssl);
    socket_close(Cofd);
    Cssl = Sssl = NULL;
    Cofd = 0;

    syslog(Log_Type, "[%s] session end.", ClientIPaddr);

    // モジュールの終了処理
    DEBUG_MODE print_message("モジュールの終了処理．(%d)\n", getpid());
    if (!term_process(Sofd)) {
        syslog(Log_Type, "module termination error.");
        print_message("モジュールの終了処理の失敗．(%d)\n", getpid());
        exit(1);
    }

    socket_close(Sofd);
    DEBUG_MODE print_message("子プロセスの終了．(%d)\n", getpid());

    exit(0);
}
 


//
// プログラムの終了
//
void  sig_term(int signal)
{
    UNUSED(signal);

    pid_t pid = getpid();
    if (pid==RootPID) {
        if (term_main()) DEBUG_MODE print_message("プログラムの終了処理の失敗．(%d)\n", getpid());
    }

    if (Cofd>0) { close(Cofd); Cofd = 0;}
    if (Sofd>0) { close(Sofd); Sofd = 0;}
    if (Nofd>0) { close(Nofd); Nofd = 0;}
    if (PIDFile!=NULL) remove(PIDFile);

    closelog(); // close syslog 
    //exit(signal);
    exit(0);
}



//
// child プロセスの終了
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



