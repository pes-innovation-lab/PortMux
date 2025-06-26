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

using namespace std;

#define MAX_EVENTS 50
#define READ_SIZE 1024
#define PORT 8080
#define SERVER "0.0.0.0"

void convert_to_non_blocking(int);
void handle_disconnect(int, int);
string resolve_ip(int);
void parseRequest(string, int, bool, int);
int create_service_connection(const string &);
int parse_service_response(int, string);

// IP: {fd: service?}

map<int, int> service_to_client;
map<int, int> client_to_service;
map<int, string> fd_to_service_name;

map<string, int> service_mapping = {
    {"SSH", 6969},
    {"HTTP", 6970}};

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
            string message_for_client;
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
                cout << "[+] New client connected! " << "(FD: " << client_fd << ")" << endl;
            }

            else if (service_to_client.find(events[i].data.fd) != service_to_client.end())
            {
                int service_fd = events[i].data.fd;
                int client_fd = service_to_client[service_fd];

                char service_buffer[READ_SIZE + 1] = {0};
                ssize_t service_bytes = recv(service_fd, service_buffer, sizeof(service_buffer) - 1, 0);

                if (service_bytes <= 0)
                {
                    // handle service disconnect, if necessary
                    cout << "[!] Service disconnected (FD: " << service_fd << ")" << endl;
                    handle_disconnect(client_fd, epoll_fd);
                    continue;
                }

                cout << "Received this from service with FD:" << service_fd << " " << service_buffer << endl;
                ssize_t sent = parse_service_response(service_fd, string(service_buffer, service_bytes));
                // ssize_t sent = send(client_fd, service_buffer, service_bytes, 0);
                if (sent <= 0)
                {
                    cout << "[-] Failed to send response to client! " << endl;
                    handle_disconnect(client_fd, epoll_fd);
                    continue;
                }
            }

            else
            {
                // Connection is already setup.

                bool isFirstMessage = (fd_to_service_name.find(events[i].data.fd) == fd_to_service_name.end());

                // See if anything was sent from the client.
                memset(read_buffer, 0, sizeof(read_buffer));
                bytes_read = recv(events[i].data.fd, read_buffer, sizeof(read_buffer) - 1, 0);

                if (bytes_read <= 0)
                {
                    handle_disconnect(events[i].data.fd, epoll_fd);
                    continue;
                }

                read_buffer[bytes_read] = '\0';

                // Send it to the appropriate service !
                parseRequest(string(read_buffer, bytes_read), events[i].data.fd, isFirstMessage, epoll_fd);
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

void handle_disconnect(int fd, int epoll_fd)
{
    cout << "[!] A client with FD: " << fd << " disconnected! " << endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr); // Removing it from interest list
    close(fd);

    if (client_to_service.find(fd) != client_to_service.end())
    {
        // This is a client socket
        int client_fd = fd;
        int service_fd = client_to_service[client_fd];

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, service_fd, nullptr);
        close(service_fd);

        service_to_client.erase(service_fd);
        client_to_service.erase(client_fd);
        fd_to_service_name.erase(client_fd);

        cout << "[!] Client with FD: " << client_fd << " has disconnected!";
    }
    else if (service_to_client.find(fd) != service_to_client.end())
    {
        // This is a service socket
        int service_fd = fd;
        int client_fd = service_to_client[service_fd];

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);

        // Remove mappings
        service_to_client.erase(service_fd);
        client_to_service.erase(client_fd);
        fd_to_service_name.erase(client_fd);
    }
}

void parseRequest(string req_str, int fd, bool isFirstMessage, int epoll_fd)
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
            service.pop_back();
        }
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' '))
        {
            message.pop_back();
        }

        // Creating connection to the service for a specific client
        int service_fd = create_service_connection(service);
        cout << "New Service FD: " << service_fd << endl;
        if (service_fd < 0)
        {
            cout << "[-] Could not connection to the requested service: " << service << endl;
            return;
        }

        // Storing mappings
        service_to_client.insert({service_fd, fd});
        client_to_service.insert({fd, service_fd});
        fd_to_service_name.insert({fd, service});

        // Add the socket to monitoring
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLRDHUP;
        event.data.fd = service_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, service_fd, &event);
        cout << "Mapping: Service->Client " << service_fd << "-> " << service_to_client[service_fd] << endl;
    }
    else
    {
        message = req_str;
        service = fd_to_service_name[fd];
    }

    // Send message to appropriate service
    message = message + ";" + to_string(fd);
    int service_fd = client_to_service[fd];
    ssize_t sent = send(service_fd, message.c_str(), message.size(), 0);

    if (sent <= 0)
    {
        perror("service");
        cout << "[-] Could not send the query to service!" << endl;
        handle_disconnect(fd, epoll_fd);
    }
}

int create_service_connection(const string &service)
{
    auto it = service_mapping.find(service);
    if (it == service_mapping.end())
    {
        cout << "[-] Unknown service: " << service << endl;
        return -1;
    }

    int port = it->second;
    int service_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (service_fd < 0)
    {
        perror("[-] Failed to create service socket");
        return -1;
    }

    sockaddr_in service_addr;
    service_addr.sin_family = AF_INET;
    service_addr.sin_port = htons(port);
    service_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(service_fd, (sockaddr *)&service_addr, sizeof(service_addr)) < 0)
    {
        perror("[-] Failed to connect to service");
        close(service_fd);
        return -1;
    }

    convert_to_non_blocking(service_fd);
    return service_fd;
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

int parse_service_response(int service_fd, string res_str)
{
    // Perform operations
    string message;
    string fd;
    size_t delimiter_pos = res_str.find(';');
    if (delimiter_pos == string::npos)
    {
        cout << "[-] Invalid request format - no delimiter found" << endl;
        return -1;
    }

    message = res_str.substr(0, delimiter_pos);
    fd = res_str.substr(delimiter_pos + 1);

    // Remove any trailing whitespace or newline characters
    while (!fd.empty() && (fd.back() == '\n' || fd.back() == '\r' || fd.back() == ' '))
    {
        fd.pop_back();
    }
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' '))
    {
        message.pop_back();
    }
    int fd_num;
    try
    {
        fd_num = stoi(fd);
        cout << "Client FD received: " << fd_num << endl;
    }
    catch (const std::invalid_argument &e)
    {
        cout << "[-] Invalid FD: not a number (" << fd << ")" << endl;
        return -1;
    }
    catch (const std::out_of_range &e)
    {
        cout << "[-] Invalid FD: out of range (" << fd << ")" << endl;
        return -1;
    }

    if(client_to_service.find(fd_num) == client_to_service.end()){
        // Invalid client_fd
        cout << "Invalid FD detected!" << endl; 
    }

    // Send it to that client_fd
    ssize_t sent = send(fd_num, message.c_str(), message.size(), 0);
    return sent;
}