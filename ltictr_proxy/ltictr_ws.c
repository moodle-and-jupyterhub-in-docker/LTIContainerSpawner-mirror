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

