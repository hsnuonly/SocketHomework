#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sstream>
#include <fcntl.h>
#include <map>
using namespace std;

bool login(int serverfd,struct sockaddr_in svr_addr,int& port);
bool cinready();
void chat_handler(int chatfd);
map<string,struct sockaddr_in> users;
string myid;

int main(int argc, char const *argv[])
{
	string input;

	int serverfd = socket(AF_INET,SOCK_STREAM,0);
	int udpfd = socket(AF_INET,SOCK_DGRAM,0);
	int listenfd = socket(AF_INET,SOCK_STREAM,0);
	int chatfd = -1;
	char buf[2048]={0};

	struct sockaddr_in svr_addr,udpaddr,chataddr;
	bzero(&svr_addr,sizeof(svr_addr));
	svr_addr.sin_family = AF_INET;
	svr_addr.sin_addr.s_addr = inet_addr(argv[1]);
	svr_addr.sin_port = htons(atoi(argv[2]));
	udpaddr= svr_addr;
	udpaddr.sin_port++;

	int reuse=1;
	setsockopt(serverfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	setsockopt(serverfd,SOL_SOCKET,SO_REUSEPORT,&reuse,sizeof(reuse));
	setsockopt(udpfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	setsockopt(udpfd,SOL_SOCKET,SO_REUSEPORT,&reuse,sizeof(reuse));

	int port;
	if(!login(serverfd,svr_addr,port))return 1;
	strcpy(buf,"udp");
	sendto(udpfd,buf,sizeof("udp"),0,(struct sockaddr*)&udpaddr,sizeof(udpaddr));

	bzero(&chataddr,sizeof(chataddr));
	chataddr.sin_family = AF_INET;
	chataddr.sin_addr.s_addr = INADDR_ANY;
	chataddr.sin_port = htons(port+1);
	bind(listenfd,(struct sockaddr*)&chataddr,sizeof(chataddr));
	listen(listenfd,1);

	fd_set fds;
	FD_ZERO(&fds);
	while(1){
		FD_SET(STDIN_FILENO,&fds);
		FD_SET(listenfd,&fds);
		FD_SET(udpfd,&fds);
		select(max(listenfd,udpfd)+1,&fds,0,0,0);
		if(FD_ISSET(STDIN_FILENO,&fds)){
			cin>>input;
			if(input=="list"){
				write(serverfd,"list",strlen("list"));
				int len = read(serverfd,buf,sizeof(buf));
				buf[len]=0;
				cout<<buf<<"\n";
			}
			else if(input=="post"){
				cout<<"Title(One line): ";
				string title;
				string temp;
				cin.ignore();
				getline(cin,title);
				cin.clear();
				cout<<"Content: (Type EOF=Ctrl+D to send)\n";
				string content;
				while(getline(cin,temp)){
					content+=temp+"\n";
				}
				cin.clear();
				bzero(buf,sizeof(buf));
				sprintf(buf,"post %s",title.c_str());
				write(serverfd,buf,sizeof(buf));
				strcpy(buf,content.c_str());
				write(serverfd,buf,strlen(buf));
				cout<<"Post article complete.\n";
			}
			else if(input=="read"){
				int number;
				cin>>number;
				bzero(buf,sizeof(buf));
				sprintf(buf,"read %d",number);
				write(serverfd,buf,sizeof(buf));
				int len = read(serverfd,buf,sizeof(buf));
				buf[len] = 0;
				cout<<buf<<"\n";
			}
			else if(input=="broadcast"){
				cout<<"Message(One line): ";
				string message;
				cin.ignore();
				getline(cin,message);
				bzero(buf,sizeof(buf));
				int len = sprintf(buf,"broadcast %s",message.c_str());
				write(serverfd,buf,len);
			}
			else if(input=="exit"){
				close(serverfd);
				exit(0);
			}
			else if(input=="users"){
				strcpy(buf,"users");
				write(serverfd,buf,strlen(buf));
				int len = read(serverfd,buf,sizeof(buf));
				buf[len]=0;
				cout<<buf<<"\n";
				stringstream ss(buf);
				string id,ip;
				int port;
				while(ss>>id>>ip>>port){
					struct sockaddr_in temp;
					bzero(&temp,sizeof(temp));
					temp.sin_addr.s_addr = inet_addr(ip.c_str());
					temp.sin_port = htons(port+1);
					temp.sin_family = AF_INET;
					users[id] = temp;
				}
			}
			else if(input=="chat"){
				if(chatfd>0){
					cout<<"=====Return to chat room.=====\n";
					chat_handler(chatfd);
					continue;
				}
				string id;
				cin>>id;
				struct sockaddr_in userinfo=users[id];

				chatfd = socket(AF_INET,SOCK_STREAM,0);
				if(connect(chatfd,(struct sockaddr*)&userinfo,sizeof(userinfo))>=0){
					chat_handler(chatfd);
				}
				else{
					cout<<"Connect with chat peer failed.\n";
				}
			}
			else if(input=="quit"){
				close(chatfd);
				chatfd = -1;
			}
			else if(input=="exit"){
				close(serverfd);
				exit(0);
			}
		}
		if(FD_ISSET(udpfd,&fds)){
			bzero(buf,sizeof(buf));
			if(recvfrom(udpfd,buf,sizeof(buf),0,0,0)>0){
				cout<<buf<<"\n";
			}
		}
		if(FD_ISSET(listenfd,&fds)){
			chatfd = accept(listenfd,0,0);
			chat_handler(chatfd);
		}
	}
	return 0;
}

bool login(int serverfd,struct sockaddr_in svr_addr,int& port){
	char buf[2048]={0};
	cout<<"ID: ";
	string pwd;
	cin>>myid;
	cout<<"Password: ";
	cin>>pwd;
	if(connect(serverfd,(struct sockaddr*)&svr_addr,sizeof(svr_addr))<0){
		std::cout<<"Connect failed.\n";
		return 0;
	}
	sprintf(buf,"%s %s",myid.c_str(),pwd.c_str());
	write(serverfd,buf,sizeof(buf));
	strcpy(buf,"test");
	usleep(100000);
	if(send(serverfd,buf,strlen(buf),0)>0){
		int len = read(serverfd,buf,sizeof(buf));
		buf[len]=0;
		cout<<buf<<"\n";
		port = 0;
		int i;
		for(i=0;buf[i]!=':';i++);
		for(i++;i<len;i++){
			port=port*10+buf[i]-'0';
		}
		return 1;
	}
	else return 0;
}

void chat_handler(int chatfd){
	fd_set fds;
	FD_ZERO(&fds);
	struct timeval tv;
	if(chatfd>0)cout<<"=====Start chat=====\n";
	cout<<"(Use EOF=ctrl+D to return to BBS)\n";
	while(1){
		tv={10,0};
		FD_SET(STDIN_FILENO,&fds);
		FD_SET(chatfd,&fds);
		select(chatfd+1,&fds,0,0,&tv);
		char buf[2048] = {0};
		if(FD_ISSET(STDIN_FILENO,&fds)){
			cin.ignore(0,'\n');
			cin.getline(buf,sizeof(buf));
			if(cin.eof()){
				cout<<"=====Return to BBS.=====\n";
				cin.clear();
				break;
			}
			char mes[2048];
			sprintf(mes,"%s: %s",myid.c_str(),buf);
			write(chatfd,mes,sizeof(mes));
		}
		if(FD_ISSET(chatfd,&fds)){
			if(read(chatfd,buf,sizeof(buf))>0){
				cout<<buf<<"\n";
			}
			else{
				cout<<"=====Remote quit chat.=====\n";
				close(chatfd);
				break;
			}
		}
	}
}