/*  
    JSON TEST Program 
*/


#include "tjson.h"


int main(int argc, char** argv)
{
    if (argc<2) {
        fprintf(stderr, "usage ... %s filename\n", argv[0]);
        exit(1);
    }

    tJson* json = json_parse_file(argv[1], 100);

    //print_tTree(stdout, json, "    ", TRUE);
    print_json_opt(stdout, json, "\n", "    ", TRUE);

    return 0;
}




