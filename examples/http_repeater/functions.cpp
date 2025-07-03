#include "headers.hpp"

unordered_map<int, int> service_to_client_map;
unordered_map<int, int> client_to_service_map;

void convert_to_non_blocking(int socketfd)
{
    if (fcntl(socketfd, F_SETFL, O_NONBLOCK) < 0)
    {
        cout << "Error converting serversocket to nonblocking" << endl;
        close(socketfd);
        exit(EXIT_FAILURE);
    }
}

void add_to_epoll(int socketfd, int epoll_fd, struct epoll_event epinitializer)
{
    epinitializer.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
    epinitializer.data.fd = socketfd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socketfd, &epinitializer) < 0)
    {
        cout << "Error adding serverSocket to epoll" << endl;
        close(socketfd);
        exit(EXIT_FAILURE);
    }
}

void handle_disconnect(int fd, int epoll_fd)
{
    if (client_to_service_map.find(fd) != client_to_service_map.end())
    {
        cout << "[!] A Client with FD: " << fd << " disconnected! " << endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr); // Removing it from interest list
        close(fd);
        // This is a client socket
        int client_fd = fd;
        int service_fd = client_to_service_map[client_fd];

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, service_fd, nullptr);
        close(service_fd);

        service_to_client_map.erase(service_fd);
        client_to_service_map.erase(client_fd);

        cout << "[!] Service with with FD: " << service_fd << " has disconnected!" << endl;
    }
    else if (service_to_client_map.find(fd) != service_to_client_map.end())
    {
        cout << "[!] Service with FD: " << fd << " disconnected! " << endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr); // Removing it from interest list
        close(fd);
        // This is a service socket
        int service_fd = fd;
        int client_fd = service_to_client_map[service_fd];

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);

        // Remove mappings
        service_to_client_map.erase(service_fd);
        client_to_service_map.erase(client_fd);
        cout << "[!] A Client with FD: " << client_fd << " disconnected! " << endl;
    }
}

void connect_to_service(int client_fd, int epoll_fd, struct epoll_event epinitializer)
{
    int service_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (service_fd < 0)
    {
        perror("[-] Failed to create service socket");
    }

    sockaddr_in service_addr;
    service_addr.sin_family = AF_INET;
    service_addr.sin_port = htons(HTTP_PORT);
    service_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(service_fd, (sockaddr *)&service_addr, sizeof(service_addr)) < 0)
    {
        perror("[-] Failed to connect to service");
        close(service_fd);
    }
  
    cout << "The service's fd is " << service_fd << " \n";

    convert_to_non_blocking(service_fd);

    service_to_client_map.insert({service_fd, client_fd});
    client_to_service_map.insert({client_fd, service_fd});
    add_to_epoll(service_fd, epoll_fd, epinitializer);

    return;
}
