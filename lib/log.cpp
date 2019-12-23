#include "log.h"

using namespace std;

void warn(string msg){
    cerr << msg << endl;
}

void error(int exit_code, const char *msg){
    perror(msg);
    exit(exit_code);
}