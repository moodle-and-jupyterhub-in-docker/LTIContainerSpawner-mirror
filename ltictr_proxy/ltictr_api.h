
#include "ltictr_https.h"


/** 
@code
tList_data 構造体
    int                 id;      ソケットデータ．
    int                 lv;      ポート番号．負数の場合は SSL接続
    Buffer              key;     ユーザ名．
    Buffer              val;     接続先 FQDN または IPアドレス．
    void*               ptr;     日付データ（文字列）
    int                 sz;      *ptr のサイズ．strlen(*ptr)
    struct _tList*      lst;     リストデータへのポインタ
@endcode
*/


int    api_process(int sock, SSL* ssl, tList* lproxy);
char*  get_username_api(tList* hdr);

int    get_user_api(char* uname, Buffer* res, tList* lproxy);
int    add_user_api(char* uname, Buffer  buf, tList* lproxy);
int    del_user_api(char* uname, tList*  lproxy);


