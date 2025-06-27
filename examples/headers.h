#ifndef HEADERS_H
#define HEADERS_H

#include<unordered_map>

#define HTTP_PORT 6970
#define INTERCEPTOR_PORT 8080
#define MAX_EVENTS 500

int event_count;

unordered_map<int, int> service_to_client_map;
unordered_map<int, int> client_to_service_map;

void convert_to_non_blocking(int socket_fd);
void add_to_epoll(int socket_fd, int epoll_fd, struct epoll_event ep_init);

#endif
