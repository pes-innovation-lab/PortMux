#include <bits/stdc++.h>
#include<stdio.h>
#include<unistd.h>
#include<iostream>
#include<errno.h>
#include<string.h>
#include<map>
#include<unordered_map>//hashmap!!!!!!!!!!!
#include<stdlib.h>
#include<sys/epoll.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<vector>
#include <queue>

#ifndef HEADERS_HPP
#define HEADERS_HPP

#define HTTP_PORT 6970
#define SSH_PORT 22
#define INTERCEPTOR_PORT 8080
#define MAX_EVENTS 500
#define READ_SIZE 65536

using namespace std;
struct PendingData {
    vector<char> buffer;
    size_t offset;
    int target_fd;
    
    PendingData(const char* data, size_t size, int fd) 
        : buffer(data, data + size), offset(0), target_fd(fd) {}
};

enum class Protocol {
    Unknown,
    HTTP,
    HTTPS_TLS,
    SSH,
    MQTT,
    DNS,
    SMTP,
    FTP
};

extern unordered_map<int, int> service_to_client_map;
extern unordered_map<int, int> client_to_service_map;
extern unordered_map<int, queue<PendingData>> pending_writes;
extern int service_port;
extern unordered_map<int, bool> client_analyzed;

void convert_to_non_blocking(int socketfd);
void add_to_epoll(int socketfd, int epoll_fd, struct epoll_event epinitializer);
void handle_disconnect(int fd, int epoll_fd);
void connect_to_service(int client_fd, int epoll_fd, struct epoll_event epinitializer);
void set_epollout(int fd, int epoll_fd);
void clear_epollout(int fd, int epoll_fd);
bool send_data_with_buffering(int fd, const char* data, size_t data_size, int epoll_fd);
void handle_pending_writes(int fd, int epoll_fd);
string resolve_ip(int client_fd);
Protocol detect_protocol(const std::vector<uint8_t>& buffer);

#endif
