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

#ifndef HEADERS_HPP
#define HEADERS_HPP

#define HTTP_PORT 6970
#define INTERCEPTOR_PORT 8080
#define MAX_EVENTS 500
#define READ_SIZE 4096

using namespace std;

extern unordered_map<int, int> service_to_client_map;
extern unordered_map<int, int> client_to_service_map;

void convert_to_non_blocking(int socket_fd);
void add_to_epoll(int socket_fd, int epoll_fd, struct epoll_event ep_init);
void handle_disconnect(int socket_fd, int epoll_fd);
void connect_to_service(int client_fd, int epoll_fd, struct epoll_event epinitializer);

#endif
