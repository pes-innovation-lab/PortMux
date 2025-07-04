#include "headers.hpp"

unordered_map<int, int> service_to_client_map;
unordered_map<int, int> client_to_service_map;
std::unordered_map<int, std::queue<PendingData>> pending_writes;

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
    epinitializer.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLET;
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
    cout << "[!] A connection with FD: " << fd << " disconnected! " << endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);

    // Clean up pending writes for this fd
    pending_writes.erase(fd);

    if (client_to_service_map.find(fd) != client_to_service_map.end())
    {
        int client_fd = fd;
        int service_fd = client_to_service_map[client_fd];

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, service_fd, nullptr);
        close(service_fd);
        pending_writes.erase(service_fd);

        service_to_client_map.erase(service_fd);
        client_to_service_map.erase(client_fd);

        cout << "[!] Client with FD: " << client_fd << " has disconnected!" << endl;
    }
    else if (service_to_client_map.find(fd) != service_to_client_map.end())
    {
        int service_fd = fd;
        int client_fd = service_to_client_map[service_fd];

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);
        pending_writes.erase(client_fd);

        service_to_client_map.erase(service_fd);
        client_to_service_map.erase(client_fd);

        cout << "[!] Service with FD: " << service_fd << " has disconnected!" << endl;
    }
}

void connect_to_service(int client_fd, int epoll_fd, struct epoll_event epinitializer)
{
    int service_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (service_fd < 0)
    {
        perror("[-] Failed to create service socket");
        handle_disconnect(client_fd, epoll_fd); 
        return;
    }

    int enable =1;
    if (setsockopt(service_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(service_fd);
        handle_disconnect(client_fd, epoll_fd); 
        exit(EXIT_FAILURE);
    }

    sockaddr_in service_addr;
    service_addr.sin_family = AF_INET;
    service_addr.sin_port = htons(HTTP_PORT);
    service_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(service_fd, (sockaddr *)&service_addr, sizeof(service_addr)) < 0)
    {
        perror("[-] Failed to connect to service");
        handle_disconnect(client_fd, epoll_fd); 
        close(service_fd);
        return;
    }

    convert_to_non_blocking(service_fd);

    service_to_client_map.insert({service_fd, client_fd});
    client_to_service_map.insert({client_fd, service_fd});
    add_to_epoll(service_fd, epoll_fd, epinitializer);
}

void set_epollout(int fd, int epoll_fd)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
    {
        perror("epoll_ctl MOD (set EPOLLOUT)");
    }
}

void clear_epollout(int fd, int epoll_fd)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
    {
        perror("epoll_ctl MOD (clear EPOLLOUT)");
    }
}

bool send_data_with_buffering(int fd, const char *data, size_t data_size, int epoll_fd)
{
    if (pending_writes.find(fd) != pending_writes.end() && !pending_writes[fd].empty())
    {
        // Since data is alr there, add it to the back.
        pending_writes[fd].emplace(data, data_size, fd);
        return true;
    }
    ssize_t sent = send(fd, data, data_size, MSG_NOSIGNAL);

    if (sent < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            // Socket buffer is full
            pending_writes[fd].emplace(data, data_size, fd);
            set_epollout(fd, epoll_fd);
            cout << "[+] Could not send" << data_size << " bytes for FD: " << fd << endl;
            return true;
        }
        else
        {
            perror("send failed");
            cout << "Errno: " << errno << endl;
            return false;
        }
    }
    else if (sent < static_cast<ssize_t>(data_size))
    {
        // partial sending case
        size_t remaining = data_size - sent;
        pending_writes[fd].emplace(data + sent, remaining, fd);
        set_epollout(fd, epoll_fd);
        cout << "[+] Partial send: sent " << sent << "/" << data_size << " bytes, queued remaining " << remaining << " bytes for fd " << fd << endl;
        return true;
    }
    else
    {
        cout << "[+] Successfully sent " << sent << " bytes to fd " << fd << endl;
        return true;
    }
}

void handle_pending_writes(int fd, int epoll_fd)
{
    if (pending_writes.find(fd) == pending_writes.end() || pending_writes[fd].empty())
    {
        // No pending data, clear EPOLLOUT
        clear_epollout(fd, epoll_fd);
        return;
    }

    auto &queue = pending_writes[fd];

    while (!queue.empty())
    {
        PendingData &pending = queue.front();
        size_t remaining = pending.buffer.size() - pending.offset;

        ssize_t sent = send(fd, pending.buffer.data() + pending.offset, remaining, MSG_NOSIGNAL);

        if (sent < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                cout << "[+] Socket " << fd << " still not ready for writing" << endl;
                return;
            }
            else
            {
                perror("send failed in handle_pending_writes");
                queue.pop();
                continue;
            }
        }
        else if (sent < static_cast<ssize_t>(remaining))
        {
            // Partial send case
            pending.offset += sent;
            cout << "[+] Partial send: sent " << sent << "/" << remaining << " bytes from pending data for fd " << fd << endl;
            return;
        }
        else
        {
            cout << "[+] Successfully sent " << sent << " bytes from pending data for fd " << fd << endl;
            queue.pop();
        }
    }

    if (queue.empty())
    {
        // All data in the queue is transmitted.
        clear_epollout(fd, epoll_fd);
        cout << "[+] All pending data sent for fd " << fd << ", cleared EPOLLOUT" << endl;
    }
}
