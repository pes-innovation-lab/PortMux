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

#include "headers.h"


void convert_to_nonblocking(int socketfd)
{
    if (fcntl(socketfd, F_SETFL, O_NONBLOCK) < 0)
    {
        cout<<"Error converting serversocket to nonblocking"<<endl;
        close(socketfd);
        exit(EXIT_FAILURE);
    }
}

void add_to_epoll(int socketfd, int epoll_fd, struct epoll_event epinitializer)
{
    epinitializer.events=EPOLLIN|EPOLLHUP|EPOLLRDHUP;
    epinitializer.data.fd=socketfd;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,socketfd,&epinitializer)<0)
    {
        cout<<"Error adding serverSocket to epoll"<<endl;
        close(socketfd);
        exit(EXIT_FAILURE);
    }
}
