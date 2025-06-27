#include "headers.h"

int main() {
  char buffer[4096] = {0};
  int bytes_read = 0;

  int epoll_fd=epoll_create(100);
  if(epoll_fd==-1)
  {
    cout<<"Error with epoll creation"<<endl;
    exit(EXIT_FAILURE);
  }

  struct epoll_event epinitializer, events_arr[MAX_EVENTS];

  int interceptor_socket=socket(AF_INET,SOCK_STREAM,0);

  sockaddr_in serverAddress;
  serverAddress.sin_family=AF_INET;
  serverAddress.sin_port=htons(INTERCEPTOR_PORT);
  serverAddress.sin_addr.s_addr=INADDR_ANY;

  convert_to_nonblocking(interceptor_socket);
  add_to_epoll(interceptor_socket,epoll_fd,epinitializer);
    
  if(bind(interceptor_socket,(struct sockaddr*)&serverAddress,sizeof(serverAddress))<0)
  {
    cout<<"Error binding interceptor_socket"<<endl;
    close(interceptor_socket);
    exit(EXIT_FAILURE);
  }

  if(listen(serverSocket,5)<0)
  {
    cout<<"Error with serverSocket trying to listen"<<endl;
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  while(interceptor_socket) {
    event_count = epoll_wait(epoll_fd, events_arr, MAX_EVENTS, 180*1000);

