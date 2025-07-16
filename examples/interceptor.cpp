#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<thread>
#include<cstring>
#include<string>
#include<arpa/inet.h>


#define INTERCEPTOR_PORT 8080
#define SERVER_PORT 6969
using namespace std;

void handleServer(int connectionSocket, int clientSocket) {
  char buffer[1024]={0};
    

    while(connectionSocket)
    {
        memset(buffer, 0, sizeof(buffer));
        if(!(recv(clientSocket,buffer,sizeof(buffer),0))) break;
        cout<<"Msg from the og client: "<<buffer<<endl;
        
        send(connectionSocket, buffer, sizeof(buffer), 0);
    }
    //connected = 0;
    return;
}

void handleclient(int connectionSocket)
{
    char buffer[1024]={0};
    int con_suc;
    int clientSocket=socket(AF_INET,SOCK_STREAM,0);
  
    sockaddr_in serverAddress;
    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(SERVER_PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    con_suc = connect(clientSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress))==0;

    if (con_suc != -1) {
      thread t(handleServer, connectionSocket, clientSocket);
      t.detach();
    }

    while(connectionSocket&&con_suc!=-1)
    {
        memset(buffer, 0, sizeof(buffer));
        if(!(recv(connectionSocket,buffer,sizeof(buffer),0))) break;
        cout<<"Msg from the og client: "<<buffer<<endl;

        if (con_suc != -1) {
          send(clientSocket, buffer, sizeof(buffer), 0);
        } 
    }
    //connected = 0;
    return;
}

int main()
{
    int serverSocket=socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in serverAddress;
    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(INTERCEPTOR_PORT);
    serverAddress.sin_addr.s_addr=INADDR_ANY;

    bind(serverSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));

    listen(serverSocket,5);
    cout<<"\nInterceptor is listening on port "<<INTERCEPTOR_PORT<<endl;
    while(serverSocket)
    {

        int connectionSocket=accept(serverSocket,nullptr,nullptr);
        cout<<"\nConnected to a client!\n";
        thread t(handleclient,connectionSocket);
        t.detach();
    }
    return(0);
}
