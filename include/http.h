#pragma once
#include <string>
#include <cstring>
#include "socket.h"
#include "log.h"

#define IS_HTTP(req)    req.find("HTTP")!=std::string::npos

std::string get_host(std::string req);

class WebServer : public Socket{
    public:
        WebServer(std::string name, std::string resolved);
        std::string m_name;
        std::string m_resolved;
        bool m_failed;
};