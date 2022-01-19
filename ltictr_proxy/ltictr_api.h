
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


void    api_process(int sock, SSL* ssl, tList* lprxy);
char*   get_username_api(char* path);

Buffer get_info_api(char* uname, tList* lst);
int    add_user_api(char* uname, Buffer buf, tList* lst);
int    del_user_api(char* uname, tList* lst);

