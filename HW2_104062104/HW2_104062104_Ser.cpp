#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <map>

using namespace std;

class Article{
public:
	string author;
	string title;
	string addr;
	string content;
	Article(string author,string title,string addr,string content){
		this->author = author;
		this->title = title;
		this->addr = addr;
		this->content = content;
	}
};
class User{
public:
	struct sockaddr_in sockinfo;
	struct sockaddr_in udpinfo;
	string id;
	int fd;
	User(int fd,string id,struct sockaddr_in info,struct sockaddr_in udp){
		this->fd = fd;
		this->id = id;
		this->sockinfo = info;
		this->udpinfo = udp;
	}
	User(const User& another){
		*this = another;
	}
	User(){

	}
	string toString(){
		stringstream temp;
		temp<<inet_ntoa(sockinfo.sin_addr);
		temp<<" ";
		temp<<sockinfo.sin_port;
		return temp.str();
	}
};

map<string,string> password;
vector<Article> articles;
map<string,User> users;


int login(const char*,const char*);
void article_init();
void account_init();
void post(string,string,string,string);
void article_list(char* buf);
void article_read(int num,char* buf);
void user_list(char* buf);

int main(int argc, char const *argv[])
{
	int listenfd,connfd,udpfd;
	struct sockaddr_in servaddr,cli_addr;
	char buf[2048];

	fd_set tempfds,fds;
	FD_ZERO(&fds);
	account_init();
	article_init();

	struct timeval tv;
	tv.tv_sec=0;
	tv.tv_usec=0;

	listenfd = socket(AF_INET,SOCK_STREAM,0);
	udpfd = socket(AF_INET,SOCK_DGRAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[1]));
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int reuse=1;
	setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	setsockopt(listenfd,SOL_SOCKET,SO_REUSEPORT,&reuse,sizeof(reuse));
	setsockopt(udpfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	setsockopt(udpfd,SOL_SOCKET,SO_REUSEPORT,&reuse,sizeof(reuse));

	bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
	servaddr.sin_port++;
	bind(udpfd,(struct sockaddr*)&servaddr,sizeof(servaddr));


	if(listen(listenfd,10)<0){
		cout<<"Listen failed.\n";
		return 0;
	}
	int maxfdp1=max(udpfd,listenfd)+1;
	FD_SET(listenfd,&fds);
	while(1){
		tempfds=fds;
		if(select(maxfdp1,&tempfds,NULL,NULL,NULL)<=0)continue;
		if(FD_ISSET(listenfd,&tempfds)){
			unsigned int addrlen = sizeof(servaddr);
			connfd = accept(listenfd,(struct sockaddr*)&servaddr,&addrlen);
			std::cout<<inet_ntoa(servaddr.sin_addr)<<":"<<servaddr.sin_port<<" is logining."<<"\n";
			int len = read(connfd,buf,sizeof(buf));
			buf[len] = 0;
			char id[32]={0},pwd[32]={0};
			sscanf(buf,"%s %s",id,pwd);
			int logincode = login(id,pwd);
			if(logincode==-1){
				int len = sprintf(buf,"Password wrong.");
				write(connfd,buf,len);
				close(connfd);
				std::cout<<inet_ntoa(servaddr.sin_addr)<<":"<<servaddr.sin_port<<" login failed as "<<id<<"\n";
				continue;
			}
			else if(logincode==0){
				int len = sprintf(buf,"ID is already online.");
				write(connfd,buf,len);
				close(connfd);
				std::cout<<inet_ntoa(servaddr.sin_addr)<<":"<<servaddr.sin_port<<" login failed as "<<id<<"\n";
				continue;
			}
			bzero(buf,sizeof(buf));
			sprintf(buf,"Welcome, %s. You are from %s:%d",id,inet_ntoa(servaddr.sin_addr),servaddr.sin_port);
			write(connfd,buf,strlen(buf));
			usleep(100);
			socklen_t size = sizeof(cli_addr);
			recvfrom(udpfd,buf,sizeof(buf),0,(struct sockaddr*)&cli_addr,&size);
			users[id]=User(connfd,id,servaddr,cli_addr);

			std::cout<<inet_ntoa(servaddr.sin_addr)<<":"<<servaddr.sin_port<<" login success as "<<id<<"\n";
			FD_SET(connfd,&fds);
			maxfdp1=max(maxfdp1-1,connfd)+1;
		}
		else for(auto u:users){
			if(FD_ISSET(u.second.fd,&tempfds)){
				bzero(buf,sizeof(buf));
				if(read(u.second.fd,buf,sizeof(buf))>0){
					stringstream ss(buf);
					cout<<u.second.toString()<<": "<<buf<<"\n";
					string command;
					ss>>command;
					if(command=="list"){
						article_list(buf);
						write(u.second.fd,buf,sizeof(buf));
					}
					else if(command=="post"){
						string title;
						string temp;
						while(ss>>temp){
							title+=temp+" ";
						}
						int len = read(u.second.fd,buf,sizeof(buf));
						buf[len] = 0;
						post(u.second.id,title,u.second.toString(),buf);
					}
					else if(command=="read"){
						int num;
						ss>>num;
						article_read(num,buf);
						write(u.second.fd,buf,sizeof(buf));
					}
					else if(command=="broadcast"){
						string message;
						string temp;
						while(ss>>temp){
							message+=temp+" ";
						}
						bzero(buf,sizeof(buf));
						int len = sprintf(buf,"[Broadcast from %s], %s",u.second.id.c_str(),message.c_str());
						for(auto u:users){
							cli_addr = u.second.udpinfo;
							sendto(udpfd,buf,len,0,(struct sockaddr*)&cli_addr,sizeof(cli_addr));
						}
					}
					else if(command=="users"){
						user_list(buf);
						write(u.second.fd,buf,sizeof(buf));
					}
				}
				else{
					close(u.second.fd);
					FD_CLR(u.second.fd,&fds);
					cout<<u.second.toString()<<" disconnected\n";
					bool ismax = u.second.fd==maxfdp1-1;
					users.erase(u.second.id);
					if(ismax){
						maxfdp1 = max(udpfd,listenfd);
						for(auto v:users){
							maxfdp1=max(maxfdp1,v.second.fd);
						}
						maxfdp1++;
					}
					break;
				}
			}
		}
	}
	return 0;
}

int login(const char* id,const char* pwd){
	if(!password[id].empty()){
		if(password[id]==pwd){
			if(users.count(id))return 0;
			else return 1;
		}
		else return -1;
	}
	else{
		password[id] = pwd;
		ofstream ofs("account",ios::ate);
		ofs<<id<<" "<<pwd<<"\n";
		ofs.close();
		return 1;
	}
}
void account_init(){
	cout<<"Account table initializing.\n";
	ifstream ifs("account");
	string id,pwd;
	while(ifs>>id>>pwd){
		password[id]=pwd;
	}
	ifs.close();
	cout<<"Account table completely initialized.\n";
}

void article_init(){
	cout<<"Article table initializing.\n";
	ifstream ifs("Article");
	string author,title,addr,content;
	int len;
	while(ifs>>author){
		ifs.ignore(5,'\n');
		getline(ifs,title);
		ifs>>addr>>len;
		while(len--){
			char buf;
			ifs.get(buf);
			content+=buf;
		}
		articles.push_back(Article(author,title,addr,content));
	}
	ifs.close();
	cout<<"Article table completely initialized.\n";
}

void post(string author,string title,string addr,string content){
	articles.push_back(Article(author,title,addr,content));
	ofstream ofs("Article",ios::app);
	ofs<<author<<"\n"<<title<<"\n"<<addr<<"\n"<<content.length()<<"\n"<<content<<"\n";
	ofs.close();
}
void article_list(char* buf){
	stringstream temp;
	if(articles.size()==0){
		bzero(buf,sizeof(buf));
		strcpy(buf,"No article\n");
		return;
	}
	for(int i=0;i<articles.size();i++){
		temp<<i+1;
		temp<<"\t"+articles[i].author+"\t"+articles[i].title+"\t"+articles[i].addr+"\n";
	}
	bzero(buf,sizeof(buf));
	strcpy(buf,temp.str().c_str());
}


void article_read(int num,char* buf){
	bzero(buf,sizeof(buf));
	if(num>articles.size()){
		strcpy(buf,"No this article\n");
		return;
	}
	string article;
	article+="Author: "+articles[num-1].author+"\n";
	article+="Title: "+articles[num-1].title+"\n";
	article+="From: "+articles[num-1].addr+"\n";
	article+="Content:\n"+articles[num-1].content+"\n";
	strcpy(buf,article.c_str());
}

void user_list(char* buf){
	string temp;
	for(auto u:users){
		temp+=u.first+"\t"+u.second.toString()+"\n";
	}
	bzero(buf,sizeof(buf));
	strcpy(buf,temp.c_str());
}