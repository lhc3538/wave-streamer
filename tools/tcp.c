#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "../wave_streamer.h"

/**
 * @brief passive_server
 * init a tcp socket server
 * @param port
 * @param queue
 * @return
 */
int passive_server(int port,int queue)
{
    ///定义sockfd
    int server_sockfd = socket(AF_INET,SOCK_STREAM, 0);

    ///定义sockaddr_in
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(port);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    ///bind，成功返回0，出错返回-1
    if(bind(server_sockfd,(struct sockaddr *)&server_sockaddr,sizeof(server_sockaddr))==-1)
    {
        perror("bind");
        exit(1);
    }

    DBG("监听%d端口\n",port);
    ///listen，成功返回0，出错返回-1
    if(listen(server_sockfd,queue) == -1)
    {
        perror("listen");
        exit(1);
    }
    return server_sockfd;
}

/**
 * @brief connect_server
 * create a socket and connect to server
 * @param ip
 * @param port
 * @return socket fd
 */
int connect_server(char *ip,int port)
{
    ///定义sockfd
    int sock_cli = socket(AF_INET,SOCK_STREAM, 0);
    ///定义sockaddr_in
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);  ///服务器端口
    servaddr.sin_addr.s_addr = inet_addr(ip);  ///服务器ip
    DBG("连接%s:%d\n",ip,port);
    ///连接服务器，成功返回0，错误返回-1
    if (connect(sock_cli, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect server");
        return -1;
    }
    return sock_cli;
}


