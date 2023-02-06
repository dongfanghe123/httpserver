#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<string.h>
#include<sys/wait.h>
#define ISspace(x) isspace((int)(x))  //宏定义，是否是空格
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

int startup(u_short* port); //参数port指向包含要连接的端口的变量的指针

void* accept_request(void* arg);

void execute_cgi(int client,const char* path,const char* method,const char* query_string);

int get_line(int sock,char* buf,int size); //buf是存放数据的缓冲区，size是缓冲区的大小

void headers(int client,const char* filename);

void not_found(int client);

void serve_file(int client,const char* file);

void unimplemented(int client);

void cannot_execute(int client);

void error_die(const char* sc);

void cat(int client,FILE* resource);

void bad_request(int client);
