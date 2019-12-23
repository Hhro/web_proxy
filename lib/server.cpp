#include "server.h"

using namespace std;

Server::Server(int port){
    int reuseaddr_on = 1;
    socklen_t addr_len = sizeof(sockaddr);

    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(port);
    m_addr.sin_addr.s_addr = INADDR_ANY;

    m_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(m_fd < 0)
        error(1, "Failed to Create server socket.");
    if(setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on)) == -1)
        error(1, "Failed to set server socket reusable.");

    if(set_nonblock_mode() == false){
        error(1, "Failed to set non block mode");
    }

    if(bind(get_fd(), reinterpret_cast<sockaddr *>(&m_addr), addr_len) < 0)
        error(1, "Failed to bind server socket.");
    if(listen(m_fd, 5) < 0) 
        error(1, "Failed to Listen on server socket.");
}


void Server::invoke_cb_accept(int fd, short ev, void *ctx){
    return (static_cast<Server*>(ctx))->cb_accept(fd);
}

void Server::invoke_receiver(int fd, short ev, void *ctx){
    return (static_cast<Server*>(ctx))->receiver(fd);
}

void Server::cb_accept(int fd){
    int client_fd;
    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(sockaddr);
    struct event *client_evt = (struct event*)calloc(sizeof(struct event), 1);

    client_fd = accept(fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if(client_fd == -1){
        warn("[!]Accept client failed");
        return;
    }

    Client *client = new Client(client_fd, client_addr);
    if(client->set_nonblock_mode() == false){
        error(1, "Failed to set client socket as non block mode.");
    }

    m_client_mtx.lock();
    m_webclients[client_fd] = client;
    m_client_evts[client_fd] = client_evt;
    m_client_mtx.unlock();

    event_set(client_evt, client_fd, EV_READ|EV_PERSIST, invoke_receiver, this);
    event_add(client_evt, NULL);

    inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);

    std::cout << "Client from " << ip << "connected" << endl;
}

void Server::receiver(int fd){
	char buf[MSG_SZ];
	int len, wlen;
    set<int> srvs;
    Message *msg = nullptr;

    memset(buf, 0, MSG_SZ);
    len = read(fd, buf, sizeof(buf));
	if (len == 0) {
        /* Connection disconnected */
        goto close_conn;
	}
	else if (len < 0) {
        /* Unexpected socket error */
        warn("Unexpected socket error, shutdown connection");
        goto close_conn;
	}

    if(IS_HTTP(string(buf))){
        msg = new Message(fd, len, buf);
        m_msg_mtx.lock();
        m_msg_queue.push(msg);
        m_msg_mtx.unlock();
        m_msg_cv.notify_one();
    }

    return;

close_conn:
    if(is_web_server(fd))
        goto close_server_conn;

close_client_conn:
	cout << "Client disconnect" << endl;
    m_client_mtx.lock();
    close(fd);
    if(m_client_evts[fd])
        event_del(m_client_evts[fd]);
    delete(m_webclients[fd]);

    m_webclients.erase(fd);
    m_client_evts.erase(fd);
    m_client_mtx.unlock();

    m_session_mtx.lock();
    m_server_mtx.lock();
    srvs = m_sessions[fd];
    for(auto srv : srvs){
        close(srv);
        m_sess_hints.erase(srv);
        if(m_webservers[srv])
            delete(m_webservers[srv]);
        m_webservers.erase(srv);
    }
    m_sessions.erase(fd);
    m_server_mtx.unlock();
    m_session_mtx.unlock();
     
    return;

close_server_conn:
	cout << "Server disconnect" << endl;
    m_client_mtx.lock();
    close(fd);

    if(m_client_evts[fd])
        event_del(m_client_evts[fd]);
    m_client_mtx.unlock();
    m_session_mtx.lock();
    m_server_mtx.lock();
    m_sess_hints.erase(fd);
    if(m_webservers[fd])
        delete(m_webservers[fd]);
    m_webservers.erase(fd);
    m_sessions.erase(fd);
    m_session_mtx.unlock();
    m_server_mtx.unlock();
    return;  
}

void Server::msg_handler(){
    std::string http_msg;
    std::string host;
    int server_fd;
    int from_fd;
    int to_fd;
    int len;

    while(true){
        std::unique_lock<std::mutex> lk(m_msg_mtx);
        m_msg_cv.wait(lk, [&] { return !m_msg_queue.empty(); });
        Message *msg = m_msg_queue.front();
        m_msg_queue.pop();
        lk.unlock();

        from_fd = msg->get_from_fd();
        len = msg->get_len();
        http_msg = msg->get_content();

        if(is_web_server(from_fd)){
            to_fd = route_by_srv(from_fd);
            cout << "[HTTP RESPONSE]" << endl;
            cout << http_msg << endl;
        }
        else{
            host = get_host(http_msg);
            if(no_session_alive(from_fd, host)){
                make_new_session(from_fd, host);
            }
            to_fd = route_by_host(from_fd, host);
            cout << "[HTTP REQUEST]" << endl;
            cout << http_msg << endl;
        }

        m_client_mtx.lock();
        write(to_fd, http_msg.c_str(), len);
        m_client_mtx.unlock();

        delete(msg);
    }
}

bool Server::is_web_server(int fd){
    return m_webservers.find(fd) != m_webservers.end();
}

bool Server::no_session_alive(int fd, std::string host){
    if(m_webclients.find(fd) == m_webclients.end())
        return true;

    std::set<int> sessions = m_sessions[fd];

    for(auto sess : sessions){
        if(m_webservers[sess]->m_name == host){
            return false;
        }
    }
    
    return true;
}

std::string do_resolve(std::string host){
    char ip_str[INET6_ADDRSTRLEN];
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints ,0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    if(getaddrinfo(host.c_str(), NULL, &hints, &res)){
        error(1, "Failed to resolve");
    }

    inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip_str, sizeof(ip_str));

    return string(ip_str);
}

void Server::make_new_session(int client_fd, std::string host){
    std::string resolved;
    struct event *recv_evt = (struct event*)calloc(sizeof(struct event), 1);
    int wsvr_fd;

    if(m_resolved.find(host) != m_resolved.end())
        resolved = m_resolved[host];
    else{
        resolved = do_resolve(host);        
    }

    WebServer *wsrv = new WebServer(host, resolved);

    if(wsrv->m_failed){
        delete(wsrv);
        return;
    }

    wsvr_fd = wsrv->get_fd();

    event_set(recv_evt, wsvr_fd, EV_READ|EV_PERSIST, invoke_receiver, this);
    event_add(recv_evt, NULL);
    m_client_evts[wsvr_fd] = recv_evt;

    m_server_mtx.lock();
    m_session_mtx.lock();
    m_webservers[wsvr_fd] = wsrv;
    m_sessions[client_fd].insert(wsvr_fd);
    m_sess_hints[wsvr_fd] = client_fd;
    m_session_mtx.unlock();
    m_server_mtx.unlock();
}

int Server::route_by_host(int client_fd, std::string host){
    set<int> srvs = m_sessions[client_fd];

    for(auto srv : srvs){
        if(m_webservers[srv]->m_name == host){
            return m_webservers[srv]->get_fd();
        }
    }
}

int Server::route_by_srv(int server_fd){
    return m_sess_hints[server_fd];
}

void Server::run_server(){
    for(int i=0;i<MSGWORKERN;i++){
        m_msg_worker.push_back(std::thread(&Server::msg_handler, this));
    }

    event_set(&m_ev_accept, m_fd, EV_READ|EV_PERSIST, invoke_cb_accept, this);
    event_add(&m_ev_accept, NULL);
    event_dispatch();

    for(int i=0;i<MSGWORKERN;i++){
        m_msg_worker[i].join();
    }
}