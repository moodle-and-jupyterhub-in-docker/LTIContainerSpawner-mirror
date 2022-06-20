/* vi: set tabstop=4 nocindent noautoindent: */

#include "ltictr_ws.h"



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


tJson*  ws_json_parse(ringBuffer* ring)
{
    // 非断片化のテキストのみサポート
    if (ring->datasz<=0) return NULL;
    if ((*ref_ringBuffer(ring, 0))!=0x81) return NULL;

    // ヘッダデータ
    tJson* json = NULL;
    unsigned char* pp = NULL;
    unsigned char* pm = NULL;
    int pos = 0, mask, i;
    long int len = 0;

    mask = (int)((*ref_ringBuffer(ring, 1)) >> 7);
    len  = (long int)((*ref_ringBuffer(ring, 1)) & 0x7f);
    pos  = 2;
    if (len==126) {
        len = (long int)(*ref_ringBuffer(ring, 2))*256 + (long int)(*ref_ringBuffer(ring, 3));
        pos = 4;
    }
    else if (len==127) {
        len = (long int)(*ref_ringBuffer(ring, 2));
        for(i=3; i<10; i++) {
            len = len*256 + (long int)(*ref_ringBuffer(ring, i));
            pos = 10;
        }
    }

    // ボディデータ処理
    if (ring->datasz >= pos + len + mask*4) {
        //
        seek_ringBuffer(ring, pos);
        if (mask==1) pm = get_ringBuffer(ring, 4);
        pp = get_ringBuffer(ring, len);   // len + 1 Byte
        //
        if (mask==1) {
            for (i=0; i<len; i++) {
                pp[i] = pp[i] ^ pm[i%4];
            }
        }

        if (pp[0]=='{') {
            pp[len] = '\0';
            json = json_parse((char*)pp, 0);
        }
        //
        free(pp);
        free(pm);
        ring->state = JBXL_NORMAL;
    }
    else {
        ring->state = JBXL_INCOMPLETE;
    }

    return json;
}


tJson*  ws_json_client(unsigned char* mesg, int cc)
{
    tJson* json = NULL;
    tJson* jtmp = NULL;

    static ringBuffer* cring = NULL;
    if (cring==NULL) cring =  new_ringBuffer(BUFSZ);

    if (mesg[0]!=0x81 && cring->state==JBXL_NORMAL) return NULL;
    if (mesg[0]==0x81 && cring->state==JBXL_INCOMPLETE) {
        clear_ringBuffer(cring);
    }
    put_ringBuffer(cring, mesg, cc);

    jtmp = ws_json_parse(cring);
    while (jtmp != NULL) {
        json = join_json(json, &jtmp);
        jtmp = ws_json_parse(cring);
    }

    return json;
}



tJson*  ws_json_server(unsigned char* mesg, int cc)
{
    tJson* json = NULL;
    tJson* jtmp = NULL;

    static ringBuffer* sring = NULL;
    if (sring==NULL) sring =  new_ringBuffer(BUFSZ);

    if (mesg[0]!=0x81 && sring->state==JBXL_NORMAL) return NULL;
    if (mesg[0]==0x81 && sring->state==JBXL_INCOMPLETE) {
        clear_ringBuffer(sring);
    }
    put_ringBuffer(sring, mesg, cc);

    jtmp = ws_json_parse(sring);
    while (jtmp != NULL) {
        json = join_json(json, &jtmp);
        jtmp = ws_json_parse(sring);
    }

    return json;
}



/*
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
*/


/*
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
*/
