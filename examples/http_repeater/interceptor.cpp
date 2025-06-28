#include "headers.hpp"

int main()
{
  char buffer[4096] = {0};
  int bytes_read = 0;

  int epoll_fd = epoll_create(100);
  if (epoll_fd == -1)
  {
    cout << "Error with epoll creation" << endl;
    exit(EXIT_FAILURE);
  }

  struct epoll_event epinitializer, events_arr[MAX_EVENTS];

  int interceptor_socket = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(INTERCEPTOR_PORT);
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  convert_to_non_blocking(interceptor_socket);
  add_to_epoll(interceptor_socket, epoll_fd, epinitializer);

  if (bind(interceptor_socket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
  {
    cout << "Error binding interceptor_socket" << endl;
    close(interceptor_socket);
    exit(EXIT_FAILURE);
  }

  if (listen(interceptor_socket, 5) < 0)
  {
    cout << "Error with interceptor_socket trying to listen" << endl;
    close(interceptor_socket);
    exit(EXIT_FAILURE);
  }

  while (interceptor_socket)
  {
    int event_count = epoll_wait(epoll_fd, events_arr, MAX_EVENTS, 180 * 1000);

    for (int i = 0; i < event_count; i++)
    {
      if (events_arr[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
      {
        handle_disconnect(events_arr[i].data.fd, epoll_fd);
        continue;
      }
      int connection_socket;
      if (events_arr[i].data.fd == interceptor_socket)
      {
        connection_socket = accept(interceptor_socket, nullptr, nullptr);
        if (connection_socket < 0)
        {
          cout << "Client could not connect!" << endl;
          continue;
        }

        convert_to_non_blocking(connection_socket);
        add_to_epoll(connection_socket, epoll_fd, epinitializer);
        cout << "Client with fd = " << connection_socket << ", has connected!" << endl;

        connect_to_service(connection_socket, epoll_fd, epinitializer);
      }
      else if (service_to_client_map.find(events_arr[i].data.fd) != service_to_client_map.end())
      {
        int service_fd = events_arr[i].data.fd;
        int client_fd = service_to_client_map[service_fd];

        char service_buffer[READ_SIZE + 1] = {0};
        ssize_t service_bytes = recv(service_fd, service_buffer, sizeof(service_buffer) - 1, 0);

        if (service_bytes <= 0)
        {
          // handle service disconnect, if necessary
          cout << "[!] Service disconnected (FD: " << service_fd << ")" << endl;
          handle_disconnect(client_fd, epoll_fd);
          continue;
        }

        ssize_t sent = send(client_fd, service_buffer, strlen(service_buffer), 0);
        if (sent <= 0)
        {
          cout << "[-] Failed to send response to client! " << endl;
          handle_disconnect(client_fd, epoll_fd);
        }
      }
      else if (client_to_service_map.find(events_arr[i].data.fd) != client_to_service_map.end())
      {
        int client_fd = events_arr[i].data.fd;
        int service_fd = client_to_service_map[client_fd];

        char client_buffer[READ_SIZE + 1] = {0};
        ssize_t client_bytes = recv(client_fd, client_buffer, sizeof(client_buffer) - 1, 0);

        if (client_bytes <= 0)
        {
          // handle client disconnect, if necessary
          cout << "[!] client disconnected (FD: " << client_fd << ")" << endl;
          handle_disconnect(service_fd, epoll_fd);
          continue;
        }

        ssize_t sent = send(service_fd, client_buffer, strlen(client_buffer), 0);
      }
    }
  }
}