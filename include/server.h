#pragma once

#include <iostream>
#include <string>
#include <cstring>

#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <set>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <event.h>

#include "socket.h"
#include "client.h"
#include "http.h"
#include "message.h"
#include "log.h"

#define MSGWORKERN 4

#define ECHO 0
#define BROADCAST 1

#define BROADCAST_FD (-1)

class Server: public Socket{
    private:
        std::unordered_map<std::string, std::string> m_resolved = {};
        std::unordered_map<int, std::set<int>> m_sessions;    //cli_fd, srv_fds
        std::unordered_map<int, int> m_sess_hints;    //srv_fd, cli_fd
        std::unordered_map<int, Client *> m_webclients = {};
        std::unordered_map<int, struct event*> m_client_evts = {};
        std::queue<Message*> m_msg_queue= {};
        std::unordered_map<int, WebServer*> m_webservers;
        std::vector<std::thread> m_msg_worker = {};
        std::mutex m_msg_mtx = {};
        std::mutex m_client_mtx = {};
        std::mutex m_server_mtx = {};
        std::mutex m_session_mtx = {};
        std::condition_variable m_msg_cv = {};
        struct event m_ev_accept = {};

        static void invoke_cb_accept(int fd, short ev, void *ctx);
        static void invoke_receiver(int fd, short ev, void *ctx);
        void cb_accept(int fd);
        void receiver(int fd);
        void msg_handler();
        void wait_msg();
        bool is_web_client(int fd);
        bool is_web_server(int fd);
        bool no_session_alive(int fd, std::string host);
        void make_new_session(int client_fd, std::string host);
        int route_by_host(int client_fd, std::string host);
        int route_by_srv(int srv_fd); 
    public:
        explicit Server(int port);
        const struct event & get_ev_accept() const { return m_ev_accept; }
        void set_ev_accept(struct event & ev);
        void run_server();
};
