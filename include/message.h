#pragma once 
#include <string>

#define MSG_SZ  1048576

class Message{
    private:
        int m_from_fd;
        int m_len;
        std::string m_content;
    public:
        Message(int from_fd, int len, char *content):
            m_from_fd(from_fd), m_len(len){
                m_content.assign(content, content+MSG_SZ);
             }
        const int get_from_fd() const { return m_from_fd; }
        const int get_len() const { return m_len; }
        const std::string get_content() const { return m_content; }
};
