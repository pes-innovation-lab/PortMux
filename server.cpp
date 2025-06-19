#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<bits/stdc++.h>
#include<thread>

#define SERVER_PORT 6969

using namespace std;

void sockthread(int clientSocket)
{
    while(clientSocket)
        {

            char buffer[1024]={0};
            if(!(recv(clientSocket,buffer,sizeof(buffer),0))) break;
            cout<<"Message from client:"<<buffer<<endl;
        }
    return;
}

void sendmsghehe(int clientSocket)
{
    string message;
    //char message[1024];
    while(clientSocket)
    {
        cout << "send: ";
        getline(cin,message);
        //cin>>message;
        //scanf("%[^\n]s", message);
        send(clientSocket,message.c_str(),strlen(message.c_str()),0);
    }
    return;
}

int main()
{
    int serverSocket=socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in serverAddress;
    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(SERVER_PORT);
    serverAddress.sin_addr.s_addr=INADDR_ANY;

    bind(serverSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));

    listen(serverSocket,5);
    cout<<"\nServer is listening on port "<<SERVER_PORT<<endl;
    while(serverSocket)

    {
        int clientSocket=accept(serverSocket,nullptr,nullptr);
        cout<<"\nConnected to the interceptor!\n";
        thread t(sockthread,clientSocket);
        t.detach();
        thread t2(sendmsghehe,clientSocket); 
        t2.detach();
    }
    close(serverSocket);
    return 0;
}

