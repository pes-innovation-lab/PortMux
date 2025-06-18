#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<bits/stdc++.h>
#include<thread>

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
    char message[1024];
    while(clientSocket)
    {
        cout << "send: ";
        scanf("%[^\n]s", message);
        send(clientSocket,message,strlen(message),0);
    }
    return;
}

int main()
{
    int serverSocket=socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in serverAddress;
    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(6969);
    serverAddress.sin_addr.s_addr=INADDR_ANY;

    bind(serverSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));

    listen(serverSocket,5);
    while(serverSocket)

    {
        int clientSocket=accept(serverSocket,nullptr,nullptr);
        thread t(sockthread,clientSocket);
        t.detach();
        thread t2(sendmsghehe,clientSocket); 
        t2.detach();
    }
    close(serverSocket);
    return 0;
}

