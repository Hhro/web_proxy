#include "server.h"

using namespace std;

void usage(){
    cout << "Usage: ./web_proxy <tcp_port> <ssl_port>" << endl;
    cout << "Example: ./echo_server 8080 4433" << endl;
}

int main(int argc, char *argv[]){
    if(argc < 3){
        usage();
        exit(1);
    }

    event_init();
    Server *srv= new Server(atoi(argv[1]));

    srv->run_server();
    
    delete(srv);
    return 0;
}