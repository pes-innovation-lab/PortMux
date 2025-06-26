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

using namespace std;

#define SERVICE1_PORT 6969
#define SERVICE2_PORT 6970
#define INTERCEPTOR_PORT 8080
#define MAX_EVENTS 50

int event_count;
unordered_map<int,int> hashmap;
map<string,int> servicetofd={
    {"SSH", 6969},
    {"HTTP", 6970}};
vector<int> servicefds;

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

void add_to_hashmap(int clientsocketfd,int servicesocketfd,unordered_map<int,int> &hashmap)
{
    hashmap[clientsocketfd]=servicesocketfd;
}

int handle_first_message(string message, int epoll_fd, struct epoll_event epinitializer)//will have to parse the first message, find service, make mapping
{
    int delimiter_pos = message.find(';');
    if(delimiter_pos==string::npos)
    {
        cout<<"Invalid first message. Must be of form: Message;service_name"<<endl;
        return -1;
    }
    //getting message and service name ex:SSH
    string msg=message.substr(0,delimiter_pos);
    string service=message.substr(delimiter_pos+1);

    //now to remove any trailing whitespaces
    while (!service.empty() && (service.back() == '\n' || service.back() == '\r' || service.back() == ' '))
    {
        service.pop_back();
    }
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r' || msg.back() == ' '))
    {
        msg.pop_back();
    }

    //finding dest port to create new socket connection to
    int servicePort=servicetofd[service];//assume its always found for now

    //making new socket
    int toserviceSocket=socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in serviceAddress;
    serviceAddress.sin_family=AF_INET;
    serviceAddress.sin_port=htons(servicePort);
    serviceAddress.sin_addr.s_addr=INADDR_ANY;

    convert_to_nonblocking(toserviceSocket);
    int result=connect(toserviceSocket,(struct sockaddr*)&serviceAddress,sizeof(serviceAddress));
    add_to_epoll(toserviceSocket,epoll_fd,epinitializer);
    
    if (result < 0) 
    {
        if (errno != EINPROGRESS) 
        {
            cout<<"Error connecting from interceptor to service socket for new client"<<endl;
            close(toserviceSocket);
            return -1;
        }
    }
    return toserviceSocket;

}

void handle_disconnect(int socketfd,int epoll_fd, int serverSocket)
{
    cout << "[!] A client with FD: " << socketfd << " disconnected! " << endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socketfd, nullptr);

    if(!(find(servicefds.begin(),servicefds.end(),socketfd)==servicefds.end()))//ie if it IS a service socket
    {
        int flag=-1;
        for(auto& it:hashmap)
        {
            if(it.second==socketfd)
            {
                flag=1;
                close(it.first);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, it.first, nullptr);
            }
        }
        if(flag<0)
        {
            cout<<"No clients connected to this service..."<<endl;
            return;
        }
        cout << "[!] A client with FD: " << socketfd << " disconnected! " << endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socketfd, nullptr); // Removing it from interest list
        close(socketfd);
        return;
    }
    else if(socketfd==serverSocket)
    {
        vector<int> keys;
        for(auto& it:hashmap)
        {
            close(it.first);
            close(it.second);
            keys.push_back(it.first);
        }
        for (int k : keys) {
            hashmap.erase(k);
        }
    }
    else
    {
        //if client disconnected
        close(hashmap[socketfd]);
        close(socketfd);
        hashmap.erase(socketfd);
    }
}

int main()
{
    char buffer[1024]={0};
    int bytes_read=0;
    //first create epoll data structure
    int epoll_fd=epoll_create(100);
    if(epoll_fd==-1)
    {
        cout<<"Error with epoll creation"<<endl;
        exit(EXIT_FAILURE);
    }

    struct epoll_event epinitializer, events_arr[MAX_EVENTS];

    int serverSocket=socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in serverAddress;
    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(INTERCEPTOR_PORT);
    serverAddress.sin_addr.s_addr=INADDR_ANY;

    convert_to_nonblocking(serverSocket);
    add_to_epoll(serverSocket,epoll_fd,epinitializer);
    
    if(bind(serverSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress))<0)
    {
        cout<<"Error binding serverSocket"<<endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if(listen(serverSocket,5)<0)
    {
        cout<<"Error with serverSocket trying to listen"<<endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    while(serverSocket)//main loop
    {
        event_count=epoll_wait(epoll_fd,events_arr,MAX_EVENTS,180*1000);

        for(int i=0;i<event_count;i++)
        {
            if (events_arr[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                handle_disconnect(events_arr[i].data.fd, epoll_fd,serverSocket);//will not only have to close that socket, but disconnect service side socket, remove any related stored things etc
                continue;
            }
            int connectionSocket;
            if(events_arr[i].data.fd==serverSocket)//event is client trying to connect
            {
                if((connectionSocket=accept(serverSocket,nullptr,nullptr))<0)
                {
                    cout<<"Error! Client could not connect!"<<endl;
                    continue;
                }
                convert_to_nonblocking(connectionSocket);
                add_to_epoll(connectionSocket,epoll_fd,epinitializer);
                cout<<"Client with fd "<<connectionSocket<<" has connected!"<<endl;
            }
            else if(find(servicefds.begin(),servicefds.end(),events_arr[i].data.fd)!=servicefds.end())//msg from a serviceSocket
            {
                bytes_read=recv(events_arr[i].data.fd,buffer,sizeof(buffer),0);
                if(bytes_read<=0)
                {
                    cout<<"Error receiving data from service"<<endl;
                    continue;
                }
                string message(buffer,bytes_read);
                int clientSocket=-1;
                for(auto& it:hashmap)
                {
                    if(it.second==events_arr[i].data.fd)
                    {
                        clientSocket=it.first;
                        break;
                    }
                }
                if(clientSocket<0)//rare case, service cant send msg first, but still
                {
                    cout<<"No clients connected to this service..."<<endl;
                    continue;
                }
                if(send(clientSocket,message.c_str(),bytes_read,0)<=0)
                {
                    cout<<"Error sending message to client from service!!!"<<endl;
                }
                continue;
            }
            else//msg from a connectionSocket,connection alr established
            {
                bool isFirstMessage=(hashmap.find(events_arr[i].data.fd)==hashmap.end());//ie no entry for this connectionSocket in hashmap
                
                bytes_read=recv(events_arr[i].data.fd,buffer,sizeof(buffer),0);
                
                if(bytes_read<=0)
                {
                    cout<<"Error receiving data from client"<<endl;
                    continue;
                }
                string message(buffer,bytes_read);
                int serviceSocket;
                if(isFirstMessage)
                {
                    serviceSocket=handle_first_message(message,epoll_fd,epinitializer);
                    if(serviceSocket<0)
                    {
                        cout<<"Error creating service Socket"<<endl;
                    }
                    servicefds.push_back(serviceSocket);//adding service socket fd to vector, so can identify later if event triggered by service socket
                    add_to_hashmap(events_arr[i].data.fd,serviceSocket,hashmap);
                }
                else
                {
                    serviceSocket=hashmap[events_arr[i].data.fd];
                    if(send(serviceSocket,message.c_str(),bytes_read,0)<=0)
                    {
                        cout<<"Error sending message to service from client"<<endl;
                    }
                    continue;
                }
            }
        }
    }
    return 0;
}