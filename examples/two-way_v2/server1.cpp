#include <stdio.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <map>

using namespace std;

#define MAX_EVENTS 10
#define READ_SIZE 1024
#define PORT 6969
#define SERVER "0.0.0.0"

void make_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
}

void handle_client_disconnect(int client_fd, int epoll_fd) {
    cout << "[!] Client FD " << client_fd << " disconnected" << endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
}

int main() {
    int running = 1, event_count;
    char read_buffer[READ_SIZE + 1];
    map<int, int> client_message_count;
    int interceptor_fd = -1;

    int epoll_fd = epoll_create1(0);
    if (epoll_fd <= 0) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN | EPOLLRDHUP;

    // Watch stdin (fd 0) for input from the terminal
    event.data.fd = STDIN_FILENO;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event) < 0) {
        perror("epoll_ctl stdin");
        exit(EXIT_FAILURE);
    }

    // Create listening socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    make_non_blocking(server_fd);

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
        perror("epoll_ctl server_fd");
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_EVENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    cout << "[+] Server started on " << SERVER << ":" << PORT << endl;
    cout << "[*] Type messages in format: <message>;<fd> to forward to client via interceptor" << endl;

    while (running) {
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (event_count < 0) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < event_count; i++) {
            int current_fd = events[i].data.fd;

            // Handle stdin input (server sends message via interceptor)
            if (current_fd == STDIN_FILENO) {
                string input;
                getline(cin, input);

                size_t sep = input.rfind(';');
                if (sep == string::npos) {
                    cout << "[-] Invalid input format. Use <message>;<fd>" << endl;
                    continue;
                }

                string msg = input.substr(0, sep);
                string fd_str = input.substr(sep + 1);

                while (!fd_str.empty() && (fd_str.back() == '\n' || fd_str.back() == '\r' || fd_str.back() == ' '))
                    fd_str.pop_back();

                int target_fd;
                try {
                    target_fd = stoi(fd_str);
                } catch (const std::invalid_argument&) {
                    cout << "[-] Invalid FD: not a number (" << fd_str << ")" << endl;
                    continue;
                } catch (const std::out_of_range&) {
                    cout << "[-] Invalid FD: out of range (" << fd_str << ")" << endl;
                    continue;
                }

                if (interceptor_fd == -1) {
                    cout << "[-] Interceptor not connected!" << endl;
                    continue;
                }

                string formatted = msg + ";" + to_string(target_fd) + "\n";
                ssize_t sent = send(interceptor_fd, formatted.c_str(), formatted.length(), 0);
                if (sent < 0) {
                    perror("send to interceptor");
                    cout << "[-] Failed to forward message to interceptor." << endl;
                } else {
                    cout << "[>] Sent to interceptor: " << formatted;
                }

                continue;
            }

            if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                if (current_fd == interceptor_fd) {
                    cout << "[!] Interceptor disconnected" << endl;
                    interceptor_fd = -1;
                } else if (current_fd != server_fd) {
                    client_message_count.erase(current_fd);
                    handle_client_disconnect(current_fd, epoll_fd);
                }
                continue;
            }

            if (current_fd == server_fd) {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);

                if (client_fd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept");
                    }
                    continue;
                }

                make_non_blocking(client_fd);

                event.events = EPOLLIN | EPOLLRDHUP;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0) {
                    perror("epoll_ctl client_fd");
                    close(client_fd);
                    continue;
                }

                string client_ip = inet_ntoa(client_addr.sin_addr);
                int client_port = ntohs(client_addr.sin_port);

                if (interceptor_fd == -1) {
                    interceptor_fd = client_fd;
                    cout << "[+] Interceptor connected from " << client_ip << ":" << client_port << " on FD " << client_fd << endl;
                } else {
                    client_message_count[client_fd] = 0;
                    cout << "[+] New client connected from " << client_ip << ":" << client_port << " on FD " << client_fd << endl;
                }

                continue;
            }

            // Handle data from client or interceptor
            memset(read_buffer, 0, sizeof(read_buffer));
            ssize_t bytes_read = recv(current_fd, read_buffer, sizeof(read_buffer) - 1, 0);

            if (bytes_read <= 0) {
                if (current_fd == interceptor_fd) {
                    cout << "[!] Interceptor closed connection" << endl;
                    interceptor_fd = -1;
                } else {
                    if (bytes_read == 0) {
                        cout << "[!] Client FD " << current_fd << " closed connection" << endl;
                    } else {
                        perror("recv");
                    }
                    client_message_count.erase(current_fd);
                    handle_client_disconnect(current_fd, epoll_fd);
                }
                continue;
            }

            read_buffer[bytes_read] = '\0';

            string msg(read_buffer);
            string client_msg;
            while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
                msg.pop_back();

            if (current_fd == interceptor_fd) {
                size_t sep = msg.rfind(';');
                if (sep == string::npos) {
                    cout << "[-] Invalid format from interceptor: " << msg << endl;
                    continue;
                }

                client_msg = msg.substr(0, sep);
                string fd_str = msg.substr(sep + 1);

                while (!fd_str.empty() && (fd_str.back() == '\n' || fd_str.back() == '\r' || fd_str.back() == ' '))
                    fd_str.pop_back();

                int from_fd;
                try {
                    from_fd = stoi(fd_str);
                } catch (const std::invalid_argument&) {
                    cout << "[-] Invalid FD from interceptor: not a number (" << fd_str << ")" << endl;
                    continue;
                } catch (const std::out_of_range&) {
                    cout << "[-] Invalid FD from interceptor: out of range (" << fd_str << ")" << endl;
                    continue;
                }

                cout << "[Client FD " << from_fd << "] via Interceptor says: \"" << client_msg << "\"" << endl;
            } else {
                client_message_count[current_fd]++;
                cout << "[+] Received from client FD " << current_fd << ": \"" << msg << "\"" << endl;

                if (interceptor_fd == -1) {
                    cout << "[-] No interceptor connected to forward client message!" << endl;
                    continue;
                }

                string to_interceptor = msg + ";" + to_string(current_fd) + "\n";
                send(interceptor_fd, client_msg.c_str(),client_msg.length(), 0);
            }
        }
    }

    for (auto &pair : client_message_count)
        close(pair.first);
    if (interceptor_fd != -1)
        close(interceptor_fd);
    close(server_fd);
    close(epoll_fd);

    cout << "[*] Server shutdown complete." << endl;
    return 0;
}
