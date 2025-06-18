#include<unistd.h>
#include<cstring>
#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<thread>
using namespace std;

void recvmsg_client(int clientSocket) {
  char buffer[1024] = {0};
  while(clientSocket) {
    memset(buffer, 0, sizeof(buffer));
    if(!(recv(clientSocket, buffer, sizeof(buffer),0))) break;
    cout << "Message from server: " << buffer << endl;
  }
  return;
}

void sendmsg_client(int clientSocket) {
  char message[1024] = {0};
  while(clientSocket) {
    memset(message, 0, sizeof(message));
    cout << "send: ";
    scanf("%[^\n]s", message);
    send(clientSocket, message, strlen(message), 0);
  }
  return;
}

int main()
{
    int clientSocket=socket(AF_INET,SOCK_STREAM,0);
    string addr_str;
    cout << "Enter the IP address of the server: ";
    cin >> addr_str;

    sockaddr_in serverAddress;

    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(8080);

    inet_pton(AF_INET, addr_str.c_str(), &serverAddress.sin_addr);

    int con_suc = connect(clientSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));
  
   if (!con_suc) {
      thread t_send(sendmsg_client, clientSocket);
      thread t_recv(recvmsg_client, clientSocket); 

      t_send.join();
      t_recv.join();
   }

    close(clientSocket);
    cout<<"closing...";
    return 0;
}
