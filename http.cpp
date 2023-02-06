#include"http.h"
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<string.h>
#include<ctype.h>
#include<pthread.h>
#define ISspace(x) isspace((int)(x)) 
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"


void error_die(const char* sc)
{
	perror(sc);
	exit(1);
}

void bad_request(int client)
{
	char buf[1024];
	sprintf(buf,"HTTP/1.0 400 BAD REQUEST\r\n");
	send(client,buf,sizeof(buf),0);
	sprintf(buf,"Content-Type: test/html\r\n");
	send(client,buf,sizeof(buf),0);
	sprintf(buf,"\r\n");
	send(client,buf,sizeof(buf),0);
	sprintf(buf,"<P>Your browser sent a bad request,");
	send(client,buf,sizeof(buf),0);
	sprintf(buf,"Such as a POST without a Content-Length.\r\n");
}




int startup(u_short* port) //参数port指向包含要连接的端口的变量的指针
{
	int httpd=0;
	struct sockaddr_in name;

	httpd=socket(PF_INET,SOCK_STREAM,0);
	if(httpd==-1)
	{
		error_die("socket error");
	}
	
	memset(&name,0,sizeof(name));
	name.sin_family=AF_INET;
	name.sin_port=htons(*port);
	name.sin_addr.s_addr=inet_addr("192.168.201.165");
	if(bind(httpd,(struct sockaddr*)&name,sizeof(name))<0)
	{
		error_die("bind error");
	}
	int on=1;
	if(setsockopt(httpd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on))!=0)
	{
		perror("setsockopt error");
		exit(1);
	}
	if(*port==0)
	{
		socklen_t namelen=sizeof(name);
		if(getsockname(httpd,(struct sockaddr*)&name,&namelen)==-1)
		{
			error_die("getsockname error");
		}
		*port=ntohs(name.sin_port);
	}
	if(listen(httpd,5)<0)
	{
		error_die("listen error");
	}
	return (httpd);

}

void* accept_request(void* arg)
{
	int client=(intptr_t)arg;
	char buf[1024];
	int numchars;
	char method[255];
	char url[255];
	char path[512];
	size_t i,j;
	struct stat st;
	int cgi=0; //如果服务器确定这是一个cgi程序，则设为true
	char* query_string=NULL;

	//根据上面的GET请求，可以看到这边就是取第一行
	//这边都是在处理第一条HTTP信息，"GET / HTTP/1.1\n"
	numchars=get_line(client,buf,sizeof(buf));
	printf("request=%s\n",buf);
	printf("numchars=%d\n",numchars);
	i=0;j=0;

	//把客户端的请求方法存到method数组
	while(!ISspace(buf[j])&&(i<sizeof(method)-1))
	{
		method[i]=buf[j];
		i++;j++;
	}
	method[i]='\0'; //结束
	printf("%s\n",method);
	//只能识别GET和POST
	if(strcasecmp(method,"GET")&&strcasecmp(method,"POST"))
	{
		unimplemented(client);
		return (void*)0;
	}

	//如果是POST，cgi置为1，即POST的时候开启cgi
	if(strcasecmp(method,"POST")==0)
	{
		cgi=1;
	}
	
	//解析并保存请求的URL(如果有问号，也包括问号及之后的内容
	i=0;
	//跳过空格
	while(ISspace(buf[j])&&(i<sizeof(buf)))
	{
		j++;
	}
	//从缓冲区中读把URL读取出来
	while(!ISspace(buf[j])&&(i<sizeof(url)-1)&&(j<sizeof(buf)))
	{
		url[i]=buf[j];
		i++;j++;
	}
	printf("url=%s\n",url);
	url[i]='\0';
	//处理GET请求
	if(strcasecmp(method,"GET")==0)
	{
		query_string=url;
		//移动指针，去找GET参数，即?后面的部分
		while((*query_string!='?')&&(*query_string!='\0'))
		{
			query_string++;
		}
		//如果找到了，说明这个请求也需要调用脚本来处理
		//此时就把请求字符串单独抽取出来
		//GET方法特点，?后面为参数
		if(*query_string=='?')
		{
			cgi=1;//开启cgi
			*query_string='\0';
			query_string++;
		}
	}
	//保存有效的URL地址并加上请求地址的主页索引，默认的根目录是在htdocs下
	//这里是做路径拼接，因为URL字符串以'/'开头，所以不能拼接新的分割符
	//格式化URL到path数组，HTML文件都在htdocs中
	sprintf(path,"/root/htdocs%s",url); //构造网页资源存放的路径
	printf("%s\n",path);
	//默认路径，解析到的路径如果为/，则自动加上index.html
	//即如果访问路径的最后一个字符是'/'，就为其补全，即默认访问index.html
	if(path[strlen(path)-1]=='/')
	{
		strcat(path,"inedx.html");
		printf("168行：%s\n",path);
	}
	//访问请求的文件，如果文件不存在直接返回，如果存在就调用cgi程序处理
	//根据路径找对应文件
	printf("172:path=%s\n",path);
	if(stat(path,&st)==-1)
	{
		//如果不存在，就把剩下的请求头从缓冲区中读出来
		//把所有headers的信息都丢弃，把所有HTTP信息读出来然后丢弃
		while((numchars>0)&&strcmp("\n",buf))
		{
			numchars=get_line(client,buf,sizeof(buf));
		}
		not_found(client);
		printf("请求的文件不存在\n");
	}
	else
	{
		//如果文件存在但是是个目录，则继续拼接路径，默认访问这个目录下的index.html
		if((st.st_mode&S_IFMT)==S_IFDIR)
		{
			strcat(path,"/index.html");
			printf("189行：%s\n",path);
		}
		printf("%lld\n",st.st_mode);
		
		long long int temp=((st.st_mode & S_IXUSR)||(st.st_mode & S_IXGRP)||(st.st_mode & S_IXOTH));
		printf("temp=%lld\n",temp);
		printf("st.st_mode&S_IXGRP=%lld\n",st.st_mode&S_IXGRP);
		printf("st.st_mode&S_IXUSR=%lld\n",st.st_mode&S_IXUSR);
		printf("st.st_mode&S_IXOTH=%lld\n",st.st_mode&S_IXOTH);
		if((st.st_mode & S_IXUSR)||(st.st_mode & S_IXGRP)||(st.st_mode & S_IXOTH))
		{
			printf("193行执行\n");
			cgi=1;
		}
		if(!cgi)
		{
			serve_file(client,path);
		}
		else
		{
			printf("execute_cgi执行\n");
			execute_cgi(client,path,method,query_string);
		}

	}
	close(client);
	return (void*)0;

}


//运行cgi程序的处理,query_string保存着GET请求时需要的参数
void execute_cgi(int client,const char* path,const char* method,const char* query_string)
{
	char buf[1024];
	int cgi_output[2];
	int cgi_input[2];
	pid_t pid;
	int status,i;
	char c;
	int numchars=1; //读取的字符数
	int content_length=-1; //HTTP的content_length，即响应正文的长度

	//首先需要根据请求是GET还是POST来分别进行处理
	//GET没有Content-Length这个项，POST有这个项
	buf[0]='A';buf[1]='\0';
	printf("228行：path=%s\n",path);
	printf("229行：%s\n",method);
	//忽略大小写比较字符
	if(strcasecmp(method,"GET")==0) //如果是GET，那么就忽略剩余的请求头
	{
		/*读取数据，把整个header都读取，因为GET直接读取index.html，没有必要分析余下的HTTP信息了，
		 * 即把所有的HTTP header读取并丢弃
		 */
		while((numchars>0)&&(strcmp("\n",buf))) //读完请求头部下的空行为之
		{
			numchars=get_line(client,buf,sizeof(buf));
		}
		printf("240行：buf=%s\n",buf);
	}
	else
	{
		/*如果是POST请求，就需要得到Content-Length,Content-Length一共长15位
		 * 所以取出头部一句后，将第16位设置结束符，进行比较第16位置为结束
		 */
		numchars=get_line(client,buf,sizeof(buf));
		while((numchars>0)&&(strcmp("\n",buf)))
		{
			//原来buf[15]应该是一个空格
			buf[15]='\0';
			if(strcasecmp(buf,"Content-length:")==0)
			{
				content_length=atoi(&(buf[16]));
			}
			numchars=get_line(client,buf,sizeof(buf));
		}
		if(content_length==-1)
		{
			bad_request(client);
			return;
		}
	}
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	send(client,buf,strlen(buf),0);
	if(pipe(cgi_output)<0)
	{
		cannot_execute(client);
		return;
	}
	
	//建立input管道
	if(pipe(cgi_input)<0)
	{
		cannot_execute(client);
		return;
	}

	if((pid=fork())<0)
	{
		cannot_execute(client);
		return;
	}
	if(pid==0) //子进程执行的内容
	{
		printf("pid==0\n");
		char meth_env[255];
		char query_env[255];
		char length_env[255];

		//把标准输出重定向到管道的写端
		dup2(cgi_output[1],1);
		//把标准输入重定向到管道的读端
		dup2(cgi_input[0],0);

		close(cgi_output[0]);
		close(cgi_input[1]);

		sprintf(meth_env,"REQUEST_METHOD=%s",method);
		putenv(meth_env);
		printf("method=%s\n",method);
		if(strcasecmp(method,"GET")==0)
		{
			printf("302行：GET\n");
			sprintf(query_env,"QUERY_STRING=%s",query_string);
			printf("query_env=%s\n",query_env);
			putenv(query_env);
			printf("putenv\n");
		}
		else
		{
			sprintf(length_env,"CONTENT_LENGTH=%d",content_length);
			putenv(length_env);
		}
		printf("313:execl将要执行\n");
		printf("316:execl要执行的path为:%s\n",path);
		execl(path,path,NULL);
		exit(0);
	}
	else //父进程执行的内容
	{
		printf("318行：父进程\n");
		close(cgi_output[1]);
		close(cgi_input[0]);
		printf("321行：method=%s\n",method);
		if(strcasecmp(method,"POST")==0)
		{
			for(i=0;i<content_length;i++)
			{
				recv(client,&c,1,0);
				write(cgi_input[1],&c,1);
			}
		}
		while(read(cgi_output[0],&c,1)>0)
		{
			printf("%c",c);
			send(client,&c,1,0);
		}
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid,&status,0);
	}

}

int get_line(int sock,char* buf,int size) //buf是存放数据的缓冲区，size是缓冲区的大小
{
	int i=0;
	char c='\0';
	int n;

	while((i<size-1)&&(c!='\n'))
	{
		n=recv(sock,&c,1,0);
		if(n>0)
		{
			//文件中的\r\n读到字符数组时转换为\n,因为windows系统文件结尾是\r\n
			if(c=='\r')
			{
				n=recv(sock,&c,1,MSG_PEEK);
				if((n>0)&&(c=='\n'))
				{
					recv(sock,&c,1,0);
				}
				c='\n';
			}
			buf[i]=c;
			i++;
		}
		else
		{
			c='\n';
		}
	}
	buf[i]='\0';
	return (i);
}

void headers(int client,const char* filename)
{
	char buf[1024];
	(void)filename;
	strcpy(buf,"HTTP/1.0 200 OK\r\n");
	send(client,buf,strlen(buf),0);
	strcpy(buf,SERVER_STRING);
	send(client,buf,strlen(buf),0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(client,buf,strlen(buf),0);
	strcpy(buf,"\r\n");
	send(client,buf,strlen(buf),0);

}

void not_found(int client)
{
	char buf[1024];
	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,SERVER_STRING);
	send(client,buf,strlen(buf),0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<BODY><P>The server could not fulfill\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"your request because the resource specified\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"is unavailable or nonexistent.\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(client,buf,strlen(buf),0);

}

void serve_file(int client,const char* filename)
{
	FILE* resource=NULL;
	int numchars=1;
	char buf[1024];

	//默认字符
	buf[0]='A';buf[1]='\0';
	while((numchars>0)&&(strcmp("\n",buf)))
	{
		numchars=get_line(client,buf,sizeof(buf));
	}
	resource=fopen(filename,"r");
	if(resource==NULL)
	{
		not_found(client);
	}
	else
	{
		headers(client,filename);
		cat(client,resource);
	}
	fclose(resource);



}


void unimplemented(int client)
{
	char buf[1024];
	sprintf(buf,"HTTP/1.0 501 Method Not Implemented\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,SERVER_STRING);
	send(client,buf,strlen(buf),0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"</TITLE></HEAD>\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<BODY><P>HTTP request method not supported.\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(client,buf,strlen(buf),0);



}


void cannot_execute(int client)
{
	char buf[1024];
	sprintf(buf,"HTTP/1.0 500 Internal Server Error\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<P>Error prohibited CGI execution.\r\n");
	send(client,buf,strlen(buf),0);
}


void server_file(int client,const char* filename)
{
	FILE* resource=NULL;
	int numchars=1;
	char buf[1024];
	
	buf[0]='A';buf[1]='\0';
	while((numchars>0)&&strcmp("\n",buf))
	{
		numchars=get_line(client,buf,sizeof(buf));
	}
	resource=fopen(filename,"r");
	if(resource==NULL)
	{
		not_found(client);
	}
	else
	{
		headers(client,filename);
		cat(client,resource);
	}
	fclose(resource);


}

void cat(int client,FILE* resource)
{
	char buf[1024];

	fgets(buf,sizeof(buf),resource);
	while(!feof(resource))
	{
		send(client,buf,strlen(buf),0);
		fgets(buf,sizeof(buf),resource);
	}
}

