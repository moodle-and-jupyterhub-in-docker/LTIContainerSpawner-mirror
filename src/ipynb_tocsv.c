/**
    ipynb_tocsv.c : CSV ファイル生成 v0.2.1 by Fumi.Iseki  BSD License.
        ipynb_extract が生成したファイル（複数）から ユーザ毎に集計された CSVファイルを生成する．

        0.2.1  2022/08/02
        0.2.0  2022/07/31
        0.1.0  2022/07/29

   ex.) ipynb_tocsv example.ipynb > example.csv
*/

#include "xtools.h"
#include "tjson.h"


char*  get_val(char* buf, const char* str);



int main(int argc, char** argv)
{
    if (argc!=2) {
        fprintf(stderr, "usage ... %s base_filename\n", argv[0]);
        exit(1);
    }

    Buffer basefn = make_Buffer_str("-");
    cat_s2Buffer(argv[1], &basefn);

    tList* ld = get_dir_files(".");
    tList* ll = ld;
    //
    while(ll!=NULL) {
        if (!strnrvscmp(ll->ldat.val.buf, basefn.buf, strlen(basefn.buf)) && !strnrvscmp(basefn.buf, ".ipynb", 6)) {
            ll->ldat.lv = ON;
        }
        else {
            ll->ldat.lv = OFF;
        }
        ll = ll->next;
    }
    
    int fnum = 0;
    //
    tList* lusr = new_tList_anchor();
    tList* lu   = lusr;

    ll = ld;
    while(ll!=NULL) {
        if (ll->ldat.lv==ON) {
            char* in_file = (char*)ll->ldat.val.buf;
            print_message("reading file %s\n", in_file);
            tJson* pp = json_parse_file(in_file, 999);
            //
            if (pp==NULL) {
                print_message("PARSE error! No json file (%s)\n", in_file);
                ll = ll->next;
                continue;
            }
            if (pp->state!=JBXL_JSON_PARSED) {
                print_message("PARSE error! (%s: %d)\n", in_file, pp->state);
                del_json(&pp);
                ll = ll->next;
                continue;
            }

            char* fname  = get_file_name(in_file);
            char* monnum = awk(fname, '-', 1);
            //print_message("checking ... %s\n", fname);
            //
            tList* ls = search_all_node_strval_json(pp, (char*)"cell_type", (char*)"code");
            //
            int count = 0;
            while (ls!=NULL) {
                if (ls->altp!=NULL) {
                    tJson* meta = search_key_sister_json(ls->altp, "metadata", FALSE);
                    if (meta!=NULL) {
                        tJson* tag = search_key_json(meta, "tags", FALSE, 1);
                        if (tag!=NULL && tag->next!=NULL) {
                            //
                            int codeflg = OFF;
                            //int score   = 0;
                            char* score = 0;
                            char* user  = NULL;
                            tJson* code = tag->next;
                            do {
                                if (code->ldat.id==JSON_ARRAY_VALUE_NODE && code->ldat.lv==JSON_VALUE_STR) {
                                    if (!strncmp("\"user: ", code->ldat.val.buf, 7)) {
                                        user = get_val((char*)code->ldat.val.buf, "\"user: ");
                                    }
                                    else if (!strncmp("\"codenum: 999\"", code->ldat.val.buf, 14)) {
                                        codeflg = ON;
                                    }
                                }
                                code = code->ysis;
                            } while(code!=NULL && !(user!=NULL && codeflg==ON));
                            //
                            if (codeflg==OFF) {
                                free_null(user);
                                user = NULL;
                            }
                            //
                            if (user!=NULL) {
                                tJson* source = search_key_sister_json(meta, "source", FALSE);
                                if (source!=NULL && source->next!=NULL) {
                                    source = source->next;
                                    if (source->ldat.id==JSON_ARRAY_VALUE_NODE && source->ldat.lv==JSON_VALUE_STR) {
                                        score = get_val((char*)source->ldat.val.buf, "\"");
                                    }
                                }
                                //
                                //print_message("%s %s %d\n", user, monnum, score);
                                lu = add_tList_node_bystr(lu, atoi(monnum), atoi(score), user, score, NULL, 0);
                                free(user);
                                free(score);
                                count++;
                            }
                        }
                    }
                }
                ls = ls->next;
            }
            del_all_tList(&ls);
            del_json(&pp);

            if (count>0) fnum++;
        }
        //
        ll = ll->next;  // 次のファイル
    }
    del_all_tList(&ld);

    int  len = sizeof(int)*fnum;
    if (len==0) {
        del_all_tList(&lusr);
        print_message("no input files.\n");
        return 1;
    }

    //
    // Title データ score_ttl[]
    int* score_ttl = (int*)malloc(len);
    memset(score_ttl, 0, len); 
    
    lu = lusr = del_tList_anchor(lusr);
    int n = 0;

    score_ttl[n] = lu->ldat.id;
    lu = lu->next;
    while (lu!=NULL) {
        if (score_ttl[n]!=lu->ldat.id) {
            score_ttl[++n] = lu->ldat.id;
        }
        lu = lu->next;
    }

    // ユーザ名で CSV データを作成
    tList* lcsv = new_tList_anchor();
    tList* lc   = lcsv;
    lu = lusr;
    while (lu!=NULL) {
        if (search_key_tList(lcsv, (const char*)lu->ldat.key.buf, 1)==NULL) {
            lc = add_tList_node_str(lc, lu->ldat.key.buf, NULL);
            //lc->ldat.sz  = len;
            //lc->ldat.ptr = (void*)malloc(len);
        }
        lu = lu->next;
    }
    lcsv = del_tList_anchor(lcsv);

    // ユーザ名でソート
    lc = lcsv;
    while (lc!=NULL) {
        tList* lv = lc->next;
        while (lv!=NULL) {
            if (strcmp((char*)lc->ldat.key.buf, (char*)lv->ldat.key.buf)>0) {
                swap_tList_node(&lc, &lv);
            }
            lv = lv->next;
        }
        lc = lc->next;
    }

    // CSV データに得点を挿入
    lc = lcsv;
    while (lc!=NULL) {
        lc->ldat.sz  = len;
        lc->ldat.ptr = (void*)malloc(len);
        memset(lc->ldat.ptr, 0, len);
        //
        lu = lusr;
        do {
            lu = search_key_tList(lu, (const char*)lc->ldat.key.buf, 1);
            if (lu!=NULL) {
                int i;
                for (i=0; i<fnum; i++) {
                    if (lu->ldat.id == score_ttl[i]) break;
                }
                if (i<fnum) {
                    ((int*)lc->ldat.ptr)[i] = lu->ldat.lv;
                }
                lu = lu->next;
            }
        } while (lu!=NULL);

        lc = lc->next;
    }

    //
    // output
    for (int i=0; i<fnum; i++) {
        fprintf(stdout, ", P%03d", score_ttl[i]);
    }
    fprintf(stdout, "\n");

    lc = lcsv;
    while (lc!=NULL) {
        fprintf(stdout, "%s", lc->ldat.key.buf);
        for (int i=0; i<fnum; i++) {
            fprintf(stdout, ", %d", ((int*)lc->ldat.ptr)[i]);
        }
        fprintf(stdout, "\n");
        lc = lc->next;
    }

    //
    free(score_ttl);
    del_all_tList(&lusr);
    del_all_tList(&lcsv);

    return 0;
}



char*  get_val(char* buf, const char* str)
{
    char* pp = buf + strlen(str);
    char* pt = pp;
    while (*pt!='\"' && *pt!='\0') pt++;
    char bk = *pt;
    *pt = '\0';
    char* name = dup_str(pp);
    *pt = bk;

    return name;
}


