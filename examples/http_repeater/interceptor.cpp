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

    int enable = 1;
    if (setsockopt(interceptor_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(interceptor_socket);
        exit(EXIT_FAILURE);
    }

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
                cout << "Disconnect is due to EPOLLERR/EPOLLHUP/EPOLLRDHUP!" << endl;
                cout << strerror(errno) << endl;
                handle_disconnect(events_arr[i].data.fd, epoll_fd);
                continue;
            }

            if (events_arr[i].events & EPOLLOUT)
            {
                cout << "Socket Number: " << events_arr[i].data.fd << " is ready to send data!" << endl;
                handle_pending_writes(events_arr[i].data.fd, epoll_fd);
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
                int enable = 1;
                if (setsockopt(connection_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                {
                    perror("setsockopt(SO_REUSEADDR) failed");
                    close(connection_socket);
                    exit(EXIT_FAILURE);
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

                // Read as much data as possible in edge-triggered mode
                char service_buffer[READ_SIZE + 1] = {0};
                ssize_t service_bytes;

                while ((service_bytes = recv(service_fd, service_buffer, sizeof(service_buffer) - 1, 0)) > 0)
                {
                    cout << "[+] Received " << service_bytes << " bytes from service FD: " << service_fd << endl;

                    // Handling partial send/block
                    if (!send_data_with_buffering(client_fd, service_buffer, service_bytes, epoll_fd))
                    {
                        cout << "[-] Failed to queue data for client" << endl;
                        handle_disconnect(client_fd, epoll_fd);
                        break;
                    }

                    memset(service_buffer, 0, sizeof(service_buffer));
                }

                if (service_bytes == 0)
                {
                    cout << "[!] Service disconnected (FD: " << service_fd << ")" << endl;
                    handle_disconnect(service_fd, epoll_fd);
                    continue;
                }
                else if (service_bytes < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
                {
                    cout << "[!] Error reading from service: " << strerror(errno) << endl;
                    handle_disconnect(service_fd, epoll_fd);
                    continue;
                }
            }
            else if (client_to_service_map.find(events_arr[i].data.fd) != client_to_service_map.end())
            {
                int client_fd = events_arr[i].data.fd;
                int service_fd = client_to_service_map[client_fd];

                // Read as much data as possible in edge-triggered mode
                char client_buffer[READ_SIZE + 1] = {0};
                ssize_t client_bytes;

                while ((client_bytes = recv(client_fd, client_buffer, sizeof(client_buffer) - 1, 0)) > 0)
                {
                    cout << "[+] Received " << client_bytes << " bytes from client FD: " << client_fd << endl;

                    if (!send_data_with_buffering(service_fd, client_buffer, client_bytes, epoll_fd))
                    {
                        cout << "[-] Failed to queue data for service" << endl;
                        handle_disconnect(client_fd, epoll_fd);
                        break;
                    }

                    memset(client_buffer, 0, sizeof(client_buffer));
                }

                if (client_bytes == 0)
                {
                    cout << "[!] Client disconnected (FD: " << client_fd << ")" << endl;
                    handle_disconnect(client_fd, epoll_fd);
                    continue;
                }
                else if (client_bytes < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
                {
                    cout << "[!] Error reading from client: " << strerror(errno) << endl;
                    handle_disconnect(client_fd, epoll_fd);
                    continue;
                }
            }
        }
    }
}