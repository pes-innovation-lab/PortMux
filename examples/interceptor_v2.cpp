#include <stdio.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <sstream>
#include <vector>
#include <queue>

// Two-way Interceptor using non-blocking code, several servers single client for now. 

using namespace std;

#define MAX_EVENTS 5
#define READ_SIZE 1024
#define PORT 8080
#define SERVER "0.0.0.0"

void convert_to_non_blocking(int);
void handle_disconnect(int, int);
string resolve_ip(int);
void parseRequest(string req_str, int fd, bool isFirstMessage);
bool is_service_socket(int);
int connect_to_services();

// IP: {fd: service?}

map<string, vector<pair<int, string>>> mapping_ip;
map<int, string> fd_to_service;
map<int, queue<int>> service_client_queue;
map<string, int> service_mapping = {
    {"SSH", 6969},
    {"HTTP", 6970}};

sockaddr_in service_addresses[5];
int service_sockets[5];

int main()
{
    int running = 1, event_count, i;
    size_t bytes_read;
    char read_buffer[READ_SIZE + 1];
    memset(read_buffer, 0, sizeof(read_buffer));

    // epoll Initialization
    int epoll_fd = epoll_create1(0);

    if (epoll_fd <= 0)
    {
        cout << "[-] Failed to create epoll FD! " << endl;
        exit(-1);
    }

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN | EPOLLRDHUP; // Monitor for read as well as remote disconnection event.

    // Server Stuff
    int connection_socket = socket(AF_INET, SOCK_STREAM, 0);

    // Setting socket reuse option
    int opt = 1;
    setsockopt(connection_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    convert_to_non_blocking(connection_socket);

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Array of structs storing services

    // Service 1
    service_addresses[0].sin_family = AF_INET;
    service_addresses[0].sin_port = htons(6969);
    service_addresses[0].sin_addr.s_addr = inet_addr("127.0.0.1");

    // Service 2
    service_addresses[1].sin_family = AF_INET;
    service_addresses[1].sin_port = htons(6970);
    service_addresses[1].sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to the service.
    service_sockets[0] = socket(AF_INET, SOCK_STREAM, 0);
    service_sockets[1] = socket(AF_INET, SOCK_STREAM, 0);

    if (service_sockets[0] < 0 || service_sockets[1] < 0)
    {
        cout << "[-] Failed to create service sockets!" << endl;
        return -1;
    }

    int service_connection = connect_to_services();
    if (service_connection < 0)
    {
        if (errno != EINPROGRESS)
        {
            cout << "[-] Could not connect to one of the services! " << endl;
            return -1; // Don't run the interceptor without it.
        }
    }

    convert_to_non_blocking(service_sockets[0]);
    convert_to_non_blocking(service_sockets[1]);

    // Monitoring service sockets using epoll
    for (int i = 0; i < 2; i++)
    {
        event.events = EPOLLIN | EPOLLRDHUP;
        event.data.fd = service_sockets[i];
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, service_sockets[i], &event);
    }

    if (bind(connection_socket, (sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        cout << "[-] Could not bind!" << endl;
        perror("bind");
        return -1;
    }

    event.data.fd = connection_socket;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connection_socket, &event) < 0)
    {
        cout << "Failed to add the file descriptor to epoll !" << endl;
        close(epoll_fd);
        exit(-1);
    }

    cout << "[+] Successfully bound to address " << SERVER << ":" << PORT << endl;

    if (listen(connection_socket, MAX_EVENTS) < 0)
    {
        cout << "[-] Cannot listen! " << endl;
        return -1;
    }

    cout << "[+] Started Listening !" << endl;
    while (running)
    {
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 30 * 1000);
        if (event_count == 0)
            continue;

        for (i = 0; i < event_count; i++)
        {
            // events[i].data.fd IS THE LISTENING SOCKET, aka, connection_socket AND NOT THE CLIENT SOCKET !!!

            if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                handle_disconnect(events[i].data.fd, epoll_fd);
                continue;
            }

            int client_fd;
            if (events[i].data.fd == connection_socket)
            {
                client_fd = accept(connection_socket, nullptr, nullptr);
                if (client_fd < 0)
                {
                    cout << "[-] Could not accept a connection! " << endl;
                    continue;
                }
                convert_to_non_blocking(client_fd);

                // Adding client socket for monitoring in epoll.
                event.events = EPOLLIN | EPOLLRDHUP;
                event.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                cout << "[+] New client connected! " << endl;
            }

            else if (is_service_socket(events[i].data.fd))
            {
                int service_fd = events[i].data.fd;
                char service_buffer[READ_SIZE + 1] = {0};
                ssize_t service_bytes = recv(service_fd, service_buffer, sizeof(service_buffer) - 1, 0);

                if (service_bytes <= 0)
                {
                    // handle service disconnect, if necessary
                    continue;
                }

                // Get the client that sent the request
                if (!service_client_queue[service_fd].empty())
                {
                    int client_fd = service_client_queue[service_fd].front();
                    service_client_queue[service_fd].pop();

                    send(client_fd, service_buffer, service_bytes, 0);
                }
                else
                {
                    cout << "[-] Got response from service but no client found!" << endl;
                }
            }

            else//client sending message
            {
                // Connection is already setup.

                bool isFirstMessage = (fd_to_service.find(events[i].data.fd) == fd_to_service.end());//ie map is empty

                memset(read_buffer, 0, sizeof(read_buffer));
                // See if anything was sent from the client.
                bytes_read = recv(events[i].data.fd, read_buffer, sizeof(read_buffer) - 1, 0);
                read_buffer[bytes_read] = '\0';
                if (bytes_read <= 0)
                {
                    handle_disconnect(events[i].data.fd, epoll_fd);
                    continue;
                }
                // Send it to the appropriate service !
                string req(read_buffer, bytes_read);
                parseRequest(string(read_buffer, bytes_read), events[i].data.fd, isFirstMessage);
                continue;
            }
        }
    }

    if (close(epoll_fd))
    {
        cout << "[-] Failed to close epoll file descriptor!" << endl;
        return -1;
    }

    return 0;
}

void convert_to_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0); // Getting the flags
    if (flags < 0)
    {
        cout << "[-] Error in fetching flags! " << endl;
        exit(-1);
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        cout << "[-] Error in setting the socket to non-blocking! " << endl;
        exit(-1);
    }
}

void handle_disconnect(int sock_fd, int epoll_fd)
{
    cout << "[!] A client with FD: " << sock_fd << " disconnected! " << endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fd, nullptr); // Removing it from interest list
    close(sock_fd);

    // Remove from fd_to_service
    fd_to_service.erase(sock_fd);

    for (auto &pair : service_client_queue)
    {
        queue<int> &q = pair.second;
        queue<int> temp_queue;

        while (!q.empty())
        {
            int client_fd = q.front();
            q.pop();
            if (client_fd != sock_fd)
            {
                temp_queue.push(client_fd);
            }
        }
        pair.second = temp_queue;
    }
}

void parseRequest(string req_str, int fd, bool isFirstMessage)
{
    string service;
    string message;

    if (isFirstMessage)
    {
        size_t delimiter_pos = req_str.find(';');
        if (delimiter_pos == string::npos)
        {
            cout << "[-] Invalid request format - no delimiter found" << endl;
            return;
        }

        message = req_str.substr(0, delimiter_pos);
        service = req_str.substr(delimiter_pos + 1);

        // Remove any trailing whitespace or newline characters
        while (!service.empty() && (service.back() == '\n' || service.back() == '\r' || service.back() == ' '))
        {
            service.pop_back();//removing extra char at the end
        }
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' '))
        {
            message.pop_back();
        }

        // Store in mapping_ip
        string client_ip = resolve_ip(fd);
        mapping_ip[client_ip].push_back({fd, service});

        // Storing in fd:service mapping
        fd_to_service[fd] = service;
    }
    else
    {
        message = req_str;
        if (fd_to_service.find(fd) == fd_to_service.end())
        {
            cout << "[-] Unknown fd without service: " << fd << endl;
            return;
        }
        service = fd_to_service[fd];
    }

    for (const auto &pair : service_mapping)//just logging
    {
        cout << "  '" << pair.first << "' -> " << pair.second << endl;
    }

    // Get corresponding port
    auto it = service_mapping.find(service);
    if (it == service_mapping.end())
    {
        cout << "[-] Service '" << service << "' not found in mapping!" << endl;
        return;
    }

    int target_port = it->second; // The port service is on.

    // Finding the corresponding service socket
    int service_fd = -1;
    for (int i = 0; i < 2; i++)
    {
        if (ntohs(service_addresses[i].sin_port) == target_port)
        {
            service_fd = service_sockets[i];
            break;
        }
    }

    if (service_fd != -1)
    {
        ssize_t sent = send(service_fd, message.c_str(), message.size(), 0);

        if (sent <= 0)
        {
            perror("[-] Send to service failed");
        }
        else
        {
            service_client_queue[service_fd].push(fd);
        }
    }
    else
    {
        cout << "[-] Could not find service socket for port " << target_port << endl;
    }
}

int connect_to_services()
{
    for (int i = 0; i < 2; i++)
    {
        int sock = service_sockets[i];

        int connection = connect(sock, (sockaddr *)&service_addresses[i], sizeof(sockaddr_in));
        if (connection < 0)
        {
            perror("connect");
            cout << "[-] Failed to connect to service on port " << ntohs(service_addresses[i].sin_port) << endl;
            return -1;
        }
        else
        {
            cout << "[+] Successfully connected to service on port " << ntohs(service_addresses[i].sin_port) << endl;
        }
    }
    return 0;
}

string resolve_ip(int fd)
{
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    int result = getpeername(fd, (struct sockaddr *)&addr, &addr_size);
    if (result < 0)
    {
        // Could not resolve into IP Address. 
        perror("getpeername");
        return "unknown";
    }
    return string(inet_ntoa(addr.sin_addr));
}

bool is_service_socket(int fd)
{
    for (int i = 0; i < 2; i++)
    {
        if (fd == service_sockets[i])
            return true;
    }
    return false;
}