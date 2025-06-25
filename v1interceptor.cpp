#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<thread>
#include<cstring>
#include<string>
#include<arpa/inet.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/epoll.h>


#define INTERCEPTOR_PORT 8080
#define SERVER_PORT 6969
#define MAX_EVENTS 100

using namespace std;

int* fdarray=(int*)malloc(sizeof(int)*100);
int fdcount=0;

void handle_disconnect(int,int);

int main()
{
    int event_count=0;
    struct epoll_event event,events[MAX_EVENTS];//defining event structure
    char buffer[1024];
    int bytes_read=0;
    int epoll_fd=epoll_create(100);//creating epoll structure, setting it to listent to 100 file descriptors 


    int clientSocket=socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in theserverAddress;
    theserverAddress.sin_family=AF_INET;
    theserverAddress.sin_port=htons(SERVER_PORT);
    theserverAddress.sin_addr.s_addr=INADDR_ANY;

    if(fcntl(clientSocket,F_SETFL,O_NONBLOCK)<0)
    {
        cout<<"error with fcntl clientsocket!!"<<endl;
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
    int con_suc = connect(clientSocket,(struct sockaddr*)&theserverAddress,sizeof(theserverAddress))==0;
    if(con_suc==-1)
    {
        cout<<"Error connecting to server..."<<endl;
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
    cout<<"Connected to server!"<<endl;
    event.data.fd=clientSocket;//tells epoll what to return when event triggers
    event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;//should EPOLLOUT Be added here for writable-ness?
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,clientSocket,&event);

    int serverSocket=socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in serverAddress;
    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(INTERCEPTOR_PORT);
    serverAddress.sin_addr.s_addr=INADDR_ANY;

    bind(serverSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));

    if(fcntl(serverSocket,F_SETFL, O_NONBLOCK)<0)//making server socket non blocking
    {
        cout<<"Error with fcntl with serversocket!!"<<endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    event.data.fd=serverSocket;
    event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;//edge trigerred notification on read, or disconnect

    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,serverSocket,&event);//adding the server/welcome socket to sockets-to-be-monitored by epoll


    listen(serverSocket,5);
    cout<<"\nInterceptor is listening on port "<<INTERCEPTOR_PORT<<endl;
    
    while(serverSocket)
    {        
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS,180*1000);//wait for events for 3 mins
        for(int i=0;i<event_count;i++)
        {
            fdarray[events[i].data.fd]=events[i].data.fd;
            fdcount++;
            if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) //if error, or disconnect
            {
                handle_disconnect(events[i].data.fd, epoll_fd);
                continue;
            }
            int connectionSocket;
            if(events[i].data.fd==serverSocket)//ie there is smn waiting at serversocket, ie thats where there is activity, ie new client gotta accept
            {
                connectionSocket=accept(serverSocket,nullptr,nullptr);
                
                if(fcntl(connectionSocket,F_SETFL, O_NONBLOCK)<0)//making connection socket non blocking
                {
                    cout<<"Error with fcntl with connection socket!!"<<endl;
                    close(serverSocket);
                    exit(EXIT_FAILURE);
                }
                event.data.fd=connectionSocket;
                event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;//edge trigerred notification on read, or disconnect
                epoll_ctl(epoll_fd,EPOLL_CTL_ADD,connectionSocket,&event);//adding the connection socket to sockets-to-be-monitored by epoll

                cout<<"\nConnected to a client!\n";
            }
            else//ie event occurred at a connectionSocket
            {
                if(events[i].data.fd!=clientSocket)//ie message incoming from client side not server side
                {
                    bytes_read=recv(events[i].data.fd,buffer,sizeof(buffer),0);
                    if(bytes_read<=0)
                    {
                        handle_disconnect(events[i].data.fd,epoll_fd);
                        continue;
                    }
                    else
                    {
                        send(clientSocket,buffer,bytes_read,0);
                        memset(buffer, 0, sizeof(buffer));
                    }
                }
                else
                {
                    bytes_read=recv(events[i].data.fd,buffer,sizeof(buffer),0);
                    if(bytes_read<=0)
                    {
                        handle_disconnect(events[i].data.fd,epoll_fd);
                        continue;
                    }
                    else
                    {
                        for(int j=0;j<fdcount;j++)
                        {
                            if(fdarray[j]!=clientSocket && fdarray[j]!=serverSocket)
                                send(fdarray[j],buffer,bytes_read,0);
                        }
                        memset(buffer, 0, sizeof(buffer));
                    }
                }
            }
            
        }
        //thread t(handleclient,connectionSocket);
        //t.detach();
    }
    close(serverSocket);
    return(0);
}


void handle_disconnect(int socketfd,int epoll_fd)
{
    cout<<"Client with socket fd:"<<socketfd<<" disconnected!!"<<endl;
    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,socketfd,nullptr);
    close(socketfd);
}