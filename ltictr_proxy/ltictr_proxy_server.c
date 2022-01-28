/*  
    Proxy Server for JupyterHub and LTIConrainerSpawner
        
                by Fumi.Iseki '22 01/22   BSD License.
*/

#include "ltictr_proxy.h"
#include "ltictr_proxy_server.h"

#define  LTICTR_PID_FILE    "LTICTR_PID_File"
#define  LTICTR_SERVER_CERT "LTICTR_Server_Cert"
#define  LTICTR_PRIVATE_KEY "LTICTR_Private_Key"
#define  LTICTR_API_TOKEN   "LTICTR_API_Token"

#define  MOODLE_HOST_KEY    "Moodle_Host"
#define  MOODLE_PORT_KEY    "Moodle_Port"
#define  MOODLE_URL_KEY     "Moodle_URL"
#define  MOODLE_TOKEN_KEY   "Moodle_Token"
#define  MOODLE_SERVICE_KEY "Moodle_Servide"
#define  MOODLE_DBANS_KEY   "Moodle_DBAns"
#define  MOODLE_TLS_KEY     "Moodle_TLS"
#define  MOODLE_HTTP_KEY    "Moodle_HTTP"

#define  API_SERVER_NAME    "ltictr_api_server"


pid_t    RootPID;

int      APIServerExec  = ON;

int      Nofd = 0, Sofd = 0;
int      ServerSSL      = OFF;     // クライアント側（自身はサーバ）とのSSL 接続
SSL_CTX* ServerCTX      = NULL;
SSL_CTX* ClientCTX      = NULL;

tList*   ProxyList      = NULL;
tList*   PIDList        = NULL;

// config file
char*    PIDFile        = "/var/run/ltictr_proxy.pid";
char*    TLS_CertPem    = "/etc/pki/tls/certs/server.pem";
char*    TLS_KeyPem     = "/etc/pki/tls/private/key.pem";
char*    API_Token      = "default_token";

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
    int    sport = 0;
    struct passwd* pw;

    ProxyList  = add_tList_node_anchor();
    PIDList    = add_tList_node_anchor();

    Buffer serverurl;
    Buffer apiurl;
    Buffer efctvuser;
    Buffer pidfile;
    Buffer certfile;
    Buffer keyfile;
    Buffer configfile;

    // for arguments
    serverurl  = init_Buffer();
    apiurl     = init_Buffer();
    efctvuser  = init_Buffer();
    pidfile    = init_Buffer();
    certfile   = init_Buffer();
    keyfile    = init_Buffer();
    configfile = init_Buffer();

    for (int i=1; i<argc; i++) {
        if      (!strcmp(argv[i],"-p")) {if (i!=argc-1) sport = atoi(argv[i+1]);}
        else if (!strcmp(argv[i],"-s")) {if (i!=argc-1) serverurl = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-a")) {if (i!=argc-1) apiurl    = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-u")) {if (i!=argc-1) efctvuser = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"-c")) ServerSSL = ON;
        else if (!strcmp(argv[i],"-d")) DebugMode = ON;

        else if (!strcmp(argv[i],"-n"))          APIServerExec = OFF;
        else if (!strcmp(argv[i],"--noexecapi")) APIServerExec = OFF; 

        else if (!strcmp(argv[i],"--pid"))    {if (i!=argc-1) pidfile    = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--cert"))   {if (i!=argc-1) certfile   = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--key"))    {if (i!=argc-1) keyfile    = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--conf"))   {if (i!=argc-1) configfile = make_Buffer_bystr(argv[i+1]);}
        else if (!strcmp(argv[i],"--config")) {if (i!=argc-1) configfile = make_Buffer_bystr(argv[i+1]);}
        //
        else if (*argv[i]=='-') print_message("[LTICTR_PROXY_SERVER] Unknown argument: %s\n", argv[i]);
    }
    if (sport==0) {
        print_message("Usage... %s -p client_side_port [-c] [-s server_url[:port]] [-a [api_url:]port] [-u user] [-d] \n", argv[0]);
        print_message("                          [--noexecapi] [--conf config_file]  [--cert cert_file] [--key key_file] [--pid pid_file]\n");
        exit(1);
    }
    //
    int cport = 0;
    if (serverurl.buf!=NULL) {
        int sz = (int)strlen((char*)serverurl.buf) - 1; 
        while(sz>=0 && serverurl.buf[sz]!='/' && serverurl.buf[sz]!=':') sz--;
        if (sz>=0 && serverurl.buf[sz]==':') {
            cport = atoi((char*)&(serverurl.buf[sz+1]));
            serverurl.buf[sz] = '\0';
            serverurl.vldsz = strlen((char*)serverurl.buf);
        }
    }
    if (cport==0) cport = sport;
    if (serverurl.buf!=NULL) {
        if (!ex_strcmp("http://", (char*)serverurl.buf) && !ex_strcmp("https://", (char*)serverurl.buf)) {
            ins_s2Buffer("http://", &serverurl);
        }
        add_tList_node_bystr(ProxyList, 0, cport, "/", (char*)serverurl.buf, NULL, 0);
        DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Target Server is %s:%d\n", (char*)serverurl.buf, cport);
    }

    //
    if (apiurl.buf!=NULL) {
        if (strstr((char*)apiurl.buf, ":")==NULL) ins_s2Buffer(":", &apiurl);
        if (!ex_strcmp("http://", (char*)apiurl.buf) && !ex_strcmp("https://", (char*)apiurl.buf)) {
            ins_s2Buffer("http://", &apiurl);
        }
        DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] API Server is %s\n", (char*)apiurl.buf);
    }
    else {
        APIServerExec = OFF;
    }

    //
    if (pidfile.buf  !=NULL) PIDFile     = (char*)pidfile.buf;
    if (certfile.buf !=NULL) TLS_CertPem = (char*)certfile.buf;
    if (keyfile.buf  !=NULL) TLS_KeyPem  = (char*)keyfile.buf;

    //
    // Initialization
    DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Start initialization.\n");
    if (configfile.buf!=NULL) {
        if (!file_exist((char*)configfile.buf)) {
            print_message("[LTICTR_PROXY_SERVER] Failure to check configuration file (%s). Can not read the configuration file.\n", (char*)configfile.buf);
            sig_term(-1);
        }
    }
    init_main(configfile);
    free_Buffer(&configfile);
    DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Initialization is finished.\n");

    //
    // Signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_term;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);      // ^C
    sigaction(SIGTERM, &sa, NULL);
    #
    set_sigterm_child(sig_child);       // Setting of proxy process is terminated

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
        DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Change to effective user (%s).\n", efctvuser.buf);
        if (isdigit(efctvuser.buf[0]) || efctvuser.buf[0]=='-') {
            err = seteuid(atoi((char*)efctvuser.buf));
        }
        else {
            pw = getpwnam((char*)efctvuser.buf);
            if (pw!=NULL) err = seteuid(pw->pw_uid);
        }
        if (err==-1) {
            DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Cannot change to effective user (%s).\n", efctvuser.buf);
        }
        free_Buffer(&efctvuser);
    }

    DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Start LTICTR_PROXY_SERVER. (%d)\n", RootPID);

    //
    // Network
    // for SSL/TLS
    if (ServerSSL==ON) {
        ssl_init();
        ServerCTX = ssl_server_setup(TLS_CertPem, TLS_KeyPem);
    }
    ClientCTX = ssl_client_setup(NULL);

    // socket open for client
    Nofd = tcp_server_socket(sport);       // block socket
    if (Nofd<0) {
        print_message("[LTICTR_PROXY_SERVER] Failure to open the server port for client connection.(%d)\n", sport);
        sig_term(-1);
    }
    DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Server port was opened for client connection. (%d)\n", sport);

    //////////////////////////////////////////////////
    // API Server Process の起動
    if (APIServerExec==ON) {
        //
        DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Start LTICTR_API_SERVER.\n");
        pid_t pid = fork();
        if (pid==0) {
            Buffer compath = make_Buffer(LPATH);
            char* path = get_file_path(argv[0]);
            if (path!=NULL) {
                copy_s2Buffer(path, &compath);
                free(path);
            }
            cat_s2Buffer(API_SERVER_NAME, &compath);
            //
            argv[0] = dup_str(API_SERVER_NAME);
            execv((char*)compath.buf, argv);
            free_Buffer(&compath);
            _exit(0);
        }
        add_tList_node_int(PIDList, (int)pid, 0);
    }

    //
    struct sockaddr cl_addr;
    socklen_t cdlen = sizeof(cl_addr);

    // main loop
    Loop {
        //Sofd = accept_intr(Nofd, &cl_addr, &cdlen);
        Sofd = accept(Nofd, &cl_addr, &cdlen);
        if (Sofd<0) {
            print_message("[LTICTR_PROXY_SERVER] Failure to connect from client. [%s]\n", strerror(errno));
            sig_term(-1);
        }
        //
        pid_t pid = fork();
        if (pid==0) receipt_proxy(Sofd, ServerCTX, ClientCTX, apiurl, ProxyList);
        close(Sofd);    // don't use socket_close() !

        tList* lp = find_tList_end(PIDList);
        add_tList_node_int(lp, (int)pid, 0);
    }

    // Unreachable
    DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Stop main loop.\n");
    term_main(99999);
    //
    exit(0);
}



int  init_main(Buffer configfile)
{
    // config file
    tList* filelist = NULL;
    if (configfile.buf!=NULL) {
        filelist = read_index_tList_file((char*)configfile.buf, '=');
        //
        if (filelist!=NULL) {
            PIDFile        = get_str_param_tList (filelist, LTICTR_PID_FILE,    PIDFile);
            TLS_CertPem    = get_str_param_tList (filelist, LTICTR_SERVER_CERT, TLS_CertPem);
            TLS_KeyPem     = get_str_param_tList (filelist, LTICTR_PRIVATE_KEY, TLS_KeyPem);
            API_Token      = get_str_param_tList (filelist, LTICTR_API_TOKEN,   API_Token);
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
                DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] The token used to connect to the Moodle Web Service has not been specified.\n");
            }
            del_tList(&filelist);
        }
    }

    return  0;
}


//
void  term_main(int code)
{
    pid_t pid = getpid();
    if (pid==RootPID) {
        socket_close(Sofd);
        socket_close(Nofd);
        if (PIDFile!=NULL) remove(PIDFile);
        //
        tList* lpid = PIDList;
        if (lpid!=NULL && lpid->ldat.id==TLIST_ANCHOR_NODE) lpid = lpid->next;
        while (lpid!=NULL) {
            if (lpid->ldat.id>0) kill((pid_t)lpid->ldat.id, SIGTERM);   
            lpid = lpid->next;   
        }
        //
        DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Shutdown root LTICTR_PROXY process with code = (%d)\n", code);
    }
    else {
        DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] Shutdown proxy LTICTR_PROXY process with code = (%d)\n", code);
    }
    return;
}


/*
void  close_all_socket(tList* lp)
{
    if (lp==NULL) return;
    if (lp->ldat.id==TLIST_ANCHOR_NODE) lp = lp->next;

    while (lp!=NULL) {
        //if (lp->ldat.id>0) close(lp->ldat.id);
        if (lp->ldat.id>0) socket_close(lp->ldat.id);
        lp = lp->next;
    }
    
    return;
}
*/


//
// Termination of program
//
void  sig_term(int signal)
{
    term_main(signal);
    
    pid_t pid = getpid();
    //DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] SIGTERM : signal = %d (%d)\n", signal, pid);

    if (signal<0) signal = -signal;
    if (signal==SIGTERM) signal = 0;    // by systemctl stop ....

    if (pid==RootPID)  exit(signal);
    else              _exit(signal);
}



//
// Termination of proxy process
//
void  sig_child(int signal)
{
    pid_t pid = 0;

    UNUSED(signal);

    int ret;
    pid = waitpid(-1, &ret, WNOHANG);
    while(pid>0) {
        tList* lst = search_id_tList(PIDList, pid, 1);
        if (lst!=NULL) del_tList_node(&lst);
        //DEBUG_MODE print_message("[LTICTR_PROXY_SERVER] SIGCHILD: signal = %d (%d)\n", signal, pid);
        //
        pid = waitpid(-1, &ret, WNOHANG);
    }

    return;
}


