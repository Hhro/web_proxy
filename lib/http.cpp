#include "http.h"

std::string get_host(std::string req){
    std::string host;
    int host_idx;
    int delim_idx;
    int host_len = 0;

    host_idx = req.find("Host: ") + 6;
    delim_idx = req.find("\r\n", host_idx);
    host_len = delim_idx - host_idx;

    host = req.substr(host_idx, host_len);

    return host;
}

WebServer::WebServer(std::string name, std::string resolved){
    m_name = name;
    m_resolved = resolved;
    m_fd = socket( PF_INET, SOCK_STREAM, 0);

    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(80);
    m_addr.sin_addr.s_addr= inet_addr(resolved.c_str());

    if( -1 == connect(m_fd, (struct sockaddr*)&m_addr, sizeof(m_addr))){
        warn("Connection failed");
        m_failed = true;
    }
}