/**  
    pynb_conv.c : ipnb ファイル 変換 v0.9.3

*/

#include "tjson.h"


int    get_codenum(char* buf);
char*  make_guid(void);
char*  make_head(char* buf, int sz, char* username);
char*  make_tail(char* buf, int sz, char* username);





int main(int argc, char** argv)
{
    if (argc<5) {
        fprintf(stderr, "usage ... %s in_filename tmp_file1 temp_file2 code_num [code_num] [code_num] ...\n", argv[0]);
        exit(1);
    }

    unsigned char* guid = NULL;
    unsigned char* user = NULL;
    unsigned char  buf[SBUFSZ];
    char* in_file;
    char* tmp_file;
    char* tmp_meta;
    tJson* js = NULL;
    FILE*  fp;

    in_file  = argv[1];
    tmp_file = argv[2];
    tmp_meta = argv[3];
    user = awk(in_file, '_', 1);

    init_rand();

    //
    tJson* pp = json_parse_file(in_file, 999);
    if (pp==NULL) exit(1);
    if (pp->state!=JBXL_JSON_PARSED) {
        print_message("PARSE error! (%s: %d)\n", in_file, pp->state);
        del_json(&pp);
        exit(1);
    }

    //
    if (!file_exist((const char*)tmp_file)) {
        fp = fopen(tmp_file, "w");
        if (fp!=NULL) {
            fprintf(fp, "{ \"cells\": [");
            fclose(fp);
        }
        else {
            print_message("File open error for write head! (%s)\n", tmp_file);
            exit(1);
        }
    }

    if (!file_exist((const char*)tmp_meta)) {
        fp = fopen(tmp_meta, "w");
        if (fp!=NULL) {
            js = search_key_json_obj(pp, "metadata", 1);
            fprintf(fp, "],\r\n");
            print_json(fp, js, JSON_INDENT_FORMAT + JSON_NOSIS_FORMAT);
            fprintf(fp, ",\r\n");

            js = search_key_json(pp, "nbformat", TRUE, 1);
            print_json(fp, js, JSON_INDENT_FORMAT + JSON_NOSIS_FORMAT);
            fprintf(fp, ",\r\n");
            js = search_key_json(pp, "nbformat_minor", TRUE, 1);
            print_json(fp, js, JSON_INDENT_FORMAT + JSON_NOSIS_FORMAT);

            fprintf(fp, "}\r\n");
            fclose(fp);
        }
        else {
            print_message("File open error for write meta! (%s)\n", tmp_file);
            exit(1);
        }
    }

    //
    tList* ls = search_all_node_strval_json(pp, (char*)"cell_type", (char*)"code");

    //
    tJson* top = new_json();
    top->ldat.id = JSON_BRACKET_NODE;
    top->ldat.lv = JSON_VALUE_OBJ;

    make_head(buf, SBUFSZ, user);
    js = json_parse(buf, 999);
    join_json(top, &js);


    while (ls!=NULL) {
        if (ls->altp!=NULL) {
            tJson* meta = search_key_sister_json(ls->altp, "metadata", FALSE);
            if (meta!=NULL) {
                tJson* tag = search_key_json(meta, "tags", FALSE, 1);
                if (tag!=NULL && tag->next!=NULL) {
                    tJson* code = tag->next;
                    do {
                        if (code->ldat.id==JSON_ARRAY_VALUE_NODE && code->ldat.lv==JSON_VALUE_STR) {
                            if (!strncmp("\"codenum: ", code->ldat.val.buf, 10)) {
                                int num = get_codenum((char*)code->ldat.val.buf);
                                for (int i=4; i<argc; i++) {
                                    if (atoi(argv[i])==num) {
                                        js = new_json();
                                        js->ldat.id = JSON_BRACKET_NODE;
                                        js->ldat.lv = JSON_VALUE_OBJ;
                                        dup_merge_json(js, ls->altp);
                                        join_json(top, &js);
                                    }

                                }
                            }
                        }
                        code = code->ysis;
                    } while(code!=NULL);
                }
            }
        }
        ls = ls->next;
    }

    make_tail(buf, SBUFSZ, user);
    js = json_parse(buf, 999);
    join_json(top, &js);

    //
    fp = fopen(tmp_file, "a");
    if (fp!=NULL) {
        print_json(fp, top->next, 2);
        fclose(fp);
    }
    else {
        print_message("File open error for add data! (%s)\n", tmp_file);
        exit(1);
    }

    del_all_tList(&ls);
    del_json(&top);
    del_json(&pp);

    return 0;
}



int  get_codenum(char* buf)
{
    char* pp = buf + 9;  // strlen("\"codenum: ") -1
    char* pt = pp;
    while (*pt!='\"' && *pt!='\0') pt++;
    char bk = *pt;
    *pt = '\0';
    int num = atoi(pp);
    *pt = bk;

    return num;
}


char*  make_guid(void)
{
    unsigned char* uuid = randbit(128);
    unsigned char* guid = uuid2guid(uuid);
    free(uuid);

    return (char*)guid;
}


char*  make_head(char* buf, int sz, char* username)
{
    char headmark[] = "{ \"cell_type\": \"markdown\", \"id\": \"%s\", \"metadata\": {}, \"source\": [ \"# %s\" ] }";
    char* guid = make_guid();

    snprintf(buf, sz, headmark, guid, username);
    free_null(guid);

    return buf;
}


char*  make_tail(char* buf, int sz, char* username)
{
    char tailmark[] = "{ \"cell_type\": \"markdown\", \"id\": \"%s\", \"metadata\": {}, \"source\": [ \"%s\" ] }";
    char codecell[] = "{ \"cell_type\": \"code\", \"execution_count\": null, \"id\": \"%s\", \
                                        \"metadata\": { \"tags\": [ \"user: %s\", \"codenum: 999\" ] }, \"source\": [\"0\"] }";
    int  len;
    char* guid;

    guid = make_guid();
    snprintf(buf, sz, tailmark, guid, "#### Score:");
    free_null(guid);

    len = strlen(buf);
    guid = make_guid();
    snprintf(buf+len, sz-len, codecell, guid, username);
    free_null(guid);

    len = strlen(buf);
    guid = make_guid();
    snprintf(buf+len, sz-len, tailmark, guid, "<hr size=\\\"2\\\">");
    free_null(guid);

    return buf;
}


