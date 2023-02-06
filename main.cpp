#include"http.h"
#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<pthread.h>
#define ISspace(x) isspace((int)(x))  //宏定义，是否是空格
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

int main(void) //服务器主函数
{
	int server_sock=-1;
	u_short port=8888; //监听端口，如果为0，则系统自动分配一个端口
	int client_sock=-1;
	struct sockaddr_in client_name;

	socklen_t client_name_len=sizeof(client_name);
	pthread_t newthread;
	
	//建立一个监听套接字，在对应的端口建立http服务
	server_sock=startup(&port);
	printf("httpd running on port %d\n",port);
	
	while(1) //无限循环
	{
		//阻塞等待客户端的连接请求
		client_sock=accept(server_sock,(struct sockaddr*)&client_name,&client_name_len);//返回一个已连接套接字
		printf("accept request\n");
		if(client_sock==-1)
		{
			error_die("accept error");
		}
		//派生线程用accept_request函数处理请求
		//每次收到请求，创建一个线程来处理接收到的请求
		//把client_sock转成地址作为参数传入pthread_create
		if(pthread_create(&newthread,NULL,accept_request,(void*)(intptr_t)client_sock)!=0)
		{
			perror("pthread_create error");
		}
	}
	close(server_sock);
	return 0;
}
