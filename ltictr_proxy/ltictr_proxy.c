/*  
    Proxy Server for JupyterHub and LTIConrainerSpawner
        
                by Fumi.Iseki '22 01/22   BSD License.
*/

#include "ltictr_proxy.h"
#include "ltictr_api.h"
#include "ltictr_https.h"
#include "ltictr_child.h"


#define  NO_SYSLOG          999

#define  LTICTR_ALLOW_FILE  "LTICTR_Allow_File"
#define  LTICTR_PID_FILE    "LTICTR_Pid_File"
#define  LTICTR_SERVER_CERT "LTICTR_Server_Cert"
#define  LTICTR_PRIVATE_KEY "LTICTR_Private_Key"

#define  MOODLE_HOST_KEY    "Moodle_Host"
#define  MOODLE_PORT_KEY    "Moodle_Port"
#define  MOODLE_URL_KEY     "Moodle_URL"
#define  MOODLE_TOKEN_KEY   "Moodle_Token"
#define  MOODLE_SERVICE_KEY "Moodle_Servide"
#define  MOODLE_DBANS_KEY   "Moodle_DBAns"
#define  MOODLE_TLS_KEY     "Moodle_TLS"
#define  MOODLE_HTTP_KEY    "Moodle_HTTP"


int      LogType;
pid_t    RootPID;

int      Nofd, Sofd;
int      Mofd, Aofd;

int      ServerSSL      = OFF;     // クライアント側（自身はサーバ）とのSSL 接続
int      APIPortSSL     = OFF;     // APIポートのSSL 接続

SSL_CTX* Server_CTX     = NULL;
SSL_CTX* Client_CTX     = NULL;
SSL_CTX* APIPort_CTX    = NULL;

tList*   ProxyList      = NULL;
tList*   AllowList      = NULL;

// config file
char*    AllowFile      = "/usr/local/etc/ltictr_allow.list";
char*    PIDFile        = "/var/run/ltictr_proxy.pid";
char*    TLS_CertPem    = "/etc/pki/tls/certs/server.pem";
char*    TLS_KeyPem     = "/etc/pki/tls/private/key.pem";

char*    Moodle_Host    = "localhost";
char*    Moodle_URL     = "/webservice/xmlrpc/server.php";
char*    Moodle_Token   = "";
char*    Moodle_Service = "mod_lticontainer_write_nbdata";
char*    Moodle_HTTP    = "1.1";
int      Moodle_Port    = 80;
int      Moodle_DBAns   = FALSE;
int      Moodle_TLS     = FALSE;



//
int main(int argc, char** argv)
{
    int  sport=0, cport=0, aport=0;
    socklen_t cdlen, pdlen;
    struct sockaddr  cl_addr, pl_addr;
    struct sigaction sa;
    struct passwd*  pw;

    Buffer hostname;
    Buffer efctvuser;
    Buffer pidfile;
    Buffer certfile;
    Buffer keyfile;
    Buffer allowfile;
    Buffer configfile;

    // for arguments
    hostname   = init_Buffer();
    efctvuser  = init_Buffer();
    pidfile    = init_Buffer();
    certfile   = init_Buffer();
    keyfile    = init_Buffer();
    configfile = init_Buffer();
    allowfile  = init_Buffer();

    for (int i=1; i<argc; i++) {
        if      (!strcmp(argv[i],"-p")) {if (i!=argc-1) sport = atoi(argv[i+1]);}
        else if (!strcmp(argv[i],"-a")) {if (i!=argc-1) aport = atoi(argv[i+1]);}
        else if (!strcmp(argv[i],"-h")) {if (i!=argc-1) hostname  = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-u")) {if (i!=argc-1) efctvuser = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-c")) ServerSSL  = ON;
        else if (!strcmp(argv[i],"-i")) APIPortSSL = ON;
        else if (!strcmp(argv[i],"-d")) DebugMode  = ON;
        //
        else if (!strcmp(argv[i],"--allow"))  {if (i!=argc-1) allowfile  = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--pid"))    {if (i!=argc-1) pidfile    = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--cert"))   {if (i!=argc-1) certfile   = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--key"))    {if (i!=argc-1) keyfile    = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--conf"))   {if (i!=argc-1) configfile = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--config")) {if (i!=argc-1) configfile = make_Buffer_bystr(argv[i+1]);}
        //
        else if (*argv[i]=='-') print_message("unknown argument: %s\n", argv[i]);
    }
    if (sport==0) {
        print_message("Usage... %s [-h host_name[:port]] -p client_side_port [-a api_port] [-s] [-c] [-i] [-u user] [-d] \n", argv[0]);
        print_message("            [--allow allow_file] [--pid pid_file] [--conf config_file]  [--cert cert_file] [--key key_file]\n");
        sig_term(-1);
    }
    //
    int i = 0;
    if (hostname.buf!=NULL) {
        while(hostname.buf[i]!='\0' && hostname.buf[i]!=':') i++;
        if (hostname.buf[i]==':') {
            cport = atoi((char*)&(hostname.buf[i+1]));
            hostname.buf[i] = '\0';
        }
    }
    if (cport==0) cport = sport;

    if (pidfile.buf!=NULL)   PIDFile     = (char*)pidfile.buf;
    if (allowfile.buf!=NULL) AllowFile   = (char*)allowfile.buf;
    if (certfile.buf!=NULL)  TLS_CertPem = (char*)certfile.buf;
    if (keyfile.buf!=NULL)   TLS_KeyPem  = (char*)keyfile.buf;

    ProxyList = add_tList_node_anchor();
    if (hostname.buf!=NULL) {
        char* lasttime = get_local_timestamp(time(0), "%Y-%b-%dT%H:%M:%SZ");
        add_tList_node_bystr(ProxyList, 0, cport, "/", (char*)hostname.buf, lasttime, strlen(lasttime)+1);
        free(lasttime);
    }

    //
    // Initialization
    DEBUG_MODE print_message("Start initialization.\n");
    LogType = init_main(configfile);
    if (LogType<0) {
        print_message("Failure to initialize.\n");
        sig_term(-1);
    }
    free_Buffer(&configfile);
    free_Buffer(&allowfile);
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
    FILE* fp = fopen((char*)PIDFile, "w");
    if (fp!=NULL) {
        fprintf(fp, "%d", (int)RootPID);
        fclose(fp);
    }

    //
    // Change effective user
    if (efctvuser.buf!=NULL) {
        int err = -1;
        DEBUG_MODE print_message("Change to effective user (%s).\n", efctvuser.buf);
        if (isdigit(efctvuser.buf[0]) || efctvuser.buf[0]=='-') {
            err = seteuid(atoi((char*)efctvuser.buf));
        }
        else {
            pw = getpwnam((char*)efctvuser.buf);
            if (pw!=NULL) err = seteuid(pw->pw_uid);
        }
        if (err==-1) {
            DEBUG_MODE print_message("Cannot change to effective user (%s).\n", efctvuser.buf);
        }
        free_Buffer(&efctvuser);
    }

    //
    // Network
    // Server API port
    if (aport!=0) {
        Mofd = tcp_server_socket(-aport);    // non block socket
        if (Mofd<0) {
            syslog(LogType, "Failure to open the api socket: [%s]", strerror(errno));
            print_message("Failure to open the api socket.\n");
            sig_term(-1);
        }
        DEBUG_MODE print_message("Port was opened for api connection. (%d)\n", aport);
    }

    // socket open for client
    Nofd = tcp_server_socket(-sport);       // non block socket
    if (Nofd<0) {
        syslog(LogType, "Failure to open the server port for client connection. [%s]", strerror(errno));
        print_message("Failure to open the server port for client connection.\n");
        sig_term(-1);
    }
    DEBUG_MODE print_message("Server port was opened for client connection. (%d)\n", sport);

    // for SSL/TLS
    if (ServerSSL==ON || APIPortSSL==ON) {
        ssl_init();
        if (ServerSSL==ON)  Server_CTX  = ssl_server_setup(TLS_CertPem, TLS_KeyPem);
        if (APIPortSSL==ON) APIPort_CTX = ssl_server_setup(TLS_CertPem, TLS_KeyPem);
    }
    Client_CTX = ssl_client_setup(NULL);

    //
    DEBUG_MODE print_message("Start main loop.\n");
    cdlen = sizeof(cl_addr);
    pdlen = sizeof(pl_addr);
    //
    fd_set mask;
    struct timeval timeout;

    Sofd = Aofd = 0;
    SSL* assl = NULL;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    // main loop
    Loop {
        if (Sofd<=0) Sofd = accept(Nofd, &cl_addr, &cdlen);
        if (Aofd<=0) Aofd = accept(Mofd, &pl_addr, &pdlen);
        int range = Max(Sofd, Aofd) + 1;

        FD_ZERO(&mask); 
        if (Sofd>0) FD_SET(Sofd, &mask);
        if (Aofd>0) FD_SET(Aofd, &mask);
        if (Aofd>0 || Sofd>0) select(range, &mask, NULL, NULL, &timeout);

        //
        if (Sofd>0 && FD_ISSET(Sofd, &mask)) {
            if (fork()==0) receipt_child(Sofd, Client_CTX, Server_CTX, ProxyList);
            close(Sofd);    // don't use socket_close() !
            Sofd = 0;
        }
        //
        if (Aofd>0 && FD_ISSET(Aofd, &mask)) {
            if (assl==NULL && APIPort_CTX!=NULL) {
                assl = ssl_server_socket(Aofd, APIPort_CTX);
                if (assl==NULL) {
                    socket_close(Aofd);
                    Aofd = 0;
                    DEBUG_MODE print_message("Unable to open SSL socket for API port. \n");
                    continue;
                }
            }
            int ret = api_process(Aofd, assl, ProxyList);
            if (ret<0) {
                ssl_close(assl);
                socket_close(Aofd);
                Aofd = 0;
                assl = NULL;
                DEBUG_MODE print_message("End of API session.\n");
            }
        }
    }

    // Unreachable
    DEBUG_MODE print_message("Stop main loop.\n");
    term_main(99999);
    //
    exit(0);
}



int  init_main(Buffer configfile)
{
    int logtype = LOG_INFO;
    //int logtype = NO_SYSLOG;

    openlog("LTICTR JupyterHub Proxy Start.", LOG_PERROR | LOG_PID, LOG_AUTH);

    // config file
    tList* filelist = NULL;
    if (configfile.buf!=NULL) {
        filelist = read_index_tList_file((char*)configfile.buf, '=');
        //
        if (filelist!=NULL) {
            PIDFile        = get_str_param_tList (filelist, LTICTR_PID_FILE,    PIDFile);
            AllowFile      = get_str_param_tList (filelist, LTICTR_ALLOW_FILE,  AllowFile);
            TLS_CertPem    = get_str_param_tList (filelist, LTICTR_SERVER_CERT, TLS_CertPem);
            TLS_KeyPem     = get_str_param_tList (filelist, LTICTR_PRIVATE_KEY, TLS_KeyPem);
            //
            Moodle_Host    = get_str_param_tList (filelist, MOODLE_HOST_KEY,    Moodle_Host);
            Moodle_URL     = get_str_param_tList (filelist, MOODLE_URL_KEY ,    Moodle_URL);
            Moodle_Token   = get_str_param_tList (filelist, MOODLE_TOKEN_KEY,   Moodle_Token);
            Moodle_Service = get_str_param_tList (filelist, MOODLE_SERVICE_KEY, Moodle_Service);
            Moodle_HTTP    = get_str_param_tList (filelist, MOODLE_HTTP_KEY,    Moodle_HTTP);
            Moodle_Port    = get_int_param_tList (filelist, MOODLE_PORT_KEY,    Moodle_Port);
            Moodle_DBAns   = get_bool_param_tList(filelist, MOODLE_DBANS_KEY,   Moodle_DBAns);
            Moodle_TLS     = get_bool_param_tList(filelist, MOODLE_TLS_KEY,     Moodle_TLS);

            if (Moodle_Token[0]=='\0') {
                DEBUG_MODE print_message("The token used to connect to the Moodle Web Service has not been specified.\n");
            }
            del_tList(&filelist);
        }
    }

    // 接続許可・禁止ファイルの読み込み
    AllowList = read_ipaddr_file(AllowFile);
    if (AllowList!=NULL) {
        DEBUG_MODE print_message("Readed access allow list\n");
    }
    else {
        DEBUG_MODE print_message("Unable to read access allow list. No access control is performed.\n");
    }

    init_xmlrpc_header();

    return  logtype;
}


//
void  term_main(int code)
{
    socket_close(Sofd);
    socket_close(Aofd);
    socket_close(Nofd);
    socket_close(Mofd);
    close_all_socket(ProxyList);

    Sofd = Aofd = Nofd = Mofd = 0;

    if (Server_CTX!=NULL)  SSL_CTX_free(Server_CTX);
    if (Client_CTX!=NULL)  SSL_CTX_free(Client_CTX);
    if (APIPort_CTX!=NULL) SSL_CTX_free(APIPort_CTX);

    //free_Buffer(&hostname);
    ////free_Buffer(&efctvuser);
    //free_Buffer(&pidfile);      // PIDFile
    //free_Buffer(&certfile);
    //free_Buffer(&keyfile);
    ////free_Buffer(&allowfile);  // AllowFile
    ////free_Buffer(&configfile);

    del_tList(&ProxyList);
    del_tList(&AllowList);

    //
    pid_t pid = getpid();
    if (pid==RootPID) {
        closelog(); // close syslog 
        if (PIDFile!=NULL) remove(PIDFile);
        //syslog(LogType, "Shutdown root ltictr_proxy process with code = (%d): [%s]", code, strerror(errno));
        print_message("Shutdown root ltictr_proxy process with code = (%d)\n", code);
    }
    else {
        DEBUG_MODE print_message("Shutdown child ltictr_proxy process with code = (%d)\n", code);
    }
    return;
}



void  close_all_socket(tList* lp)
{
    if (lp==NULL) return;

    lp = lp->next;
    while (lp!=NULL) {
        //if (lp->ldat.id>0) close(lp->ldat.id);
        if (lp->ldat.id>0) socket_close(lp->ldat.id);
        lp = lp->next;
    }
    
    return;
}



//
// Termination of program
//
void  sig_term(int signal)
{
    term_main(signal);
    
    pid_t pid = getpid();
    print_message("sig_term: Exit program with signal = %d (%d)\n", signal, pid);
    if (signal<0) exit(-signal);
    exit(signal);
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



