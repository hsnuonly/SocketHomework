#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
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
#include <fstream>
#include <thread>
#include <vector>
#include <algorithm>
#include <netdb.h>
using namespace std;

class Client{
public:
	string ip,port,id;
	map<string,bool> files;
	struct sockaddr_in info;
	Client(){

	}
	Client(const Client& c){
		*this = c;
	}
	Client(string id,struct sockaddr_in info){
		this->id = id;
		ip = inet_ntoa(info.sin_addr);
		stringstream ss;
		ss<<info.sin_port+1;
		port = ss.str();
		this->info = info;
		this->info.sin_port = info.sin_port+1;
	}
	string list(){
		stringstream ss;
		ss<<id<<" "<<ip<<" "<<port<<" ";
		for(auto f:files){
			ss<<f.first<<" ";
		}
		ss<<"| ";
		return ss.str();
	}
	void setFiles(map<string,bool> m){
		files=m;
	}
};

map<string,Client>* users;
vector<thread>* threads;
string myid = "server";

int tcpsocket();
void p2p_handler(int sockfd);
void client_handler(int sockfd);
void listfile(string path,char *buf);
int main(int argc, char const *argv[]) {
	users = new map<string,Client>();
	threads = new vector<thread>();
	mkdir(myid.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

	int listenfd = tcpsocket();
	int p2pfd = tcpsocket();
	struct sockaddr_in svraddr;
	svraddr.sin_family=AF_INET;
	svraddr.sin_port = htons(atoi(argv[1]));
	svraddr.sin_addr.s_addr = INADDR_ANY;
	bind(listenfd,(struct sockaddr*)&svraddr,sizeof(svraddr));
	svraddr.sin_port = htons(atoi(argv[1])+1);
	bind(p2pfd,(struct sockaddr*)&svraddr,sizeof(svraddr));

	int reuse = 1;
	setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	setsockopt(listenfd,SOL_SOCKET,SO_REUSEPORT,&reuse,sizeof(reuse));
	setsockopt(p2pfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	setsockopt(p2pfd,SOL_SOCKET,SO_REUSEPORT,&reuse,sizeof(reuse));

	listen(listenfd,10);
	listen(p2pfd,10);
	threads->push_back(thread(p2p_handler,p2pfd));
	(*users)["server"]=Client("server",svraddr);
	listfile(myid,0);

	while(1){
		int connfd = accept(listenfd,0,0);
		threads->push_back(thread(client_handler,connfd));
	}
	return 0;
}

void dl_handler(int sockfd){
	char buf[2048];
	read(sockfd,buf,sizeof(buf));
	string file;
	int part,size,prog=0;
	stringstream ss(buf);
	ss>>file>>part>>size;
	stringstream path;
	path<<myid<<"/"<<file<<".part"<<part;
	ofstream ofs(path.str());
	while(prog<size){
		bzero(buf,sizeof(buf));
		int n = read(sockfd,buf,sizeof(buf));
		prog+=n;
		ofs.write(buf,n);
	}
	ofs.close();
	close(sockfd);
}

void p2p_handler(int sockfd){
	while(1){
		int connfd = accept(sockfd,0,0);
		char buf[2048];
		read(connfd,buf,sizeof(buf));
		cout<<buf<<"\n";
		stringstream ss(buf);
		string com;
		ss>>com;
		if(com=="download"){
			close(connfd);
			string file;
			int part;
			ss>>file>>part;
			vector<thread> peer;
			for(int i=0;i<part;i++){
				int transfd = accept(sockfd,0,0);
				peer.push_back(thread(dl_handler,transfd));
			}
			for(int i=0;i<part;i++){
				peer[i].join();
			}
			ofstream ofs(myid+"/"+file);
			for(int i=0;i<part;i++){
				stringstream path;
				path<<myid+"/"<<file<<".part"<<i;
				ifstream ifs(path.str());
				while(ifs.tellg()>=0){
					ifs.read(buf,sizeof(buf));
					ofs.write(buf,ifs.gcount());
				}
				ifs.close();
			}
			ofs.close();
		}
		else if(com=="upload"){
			close(connfd);
			string to,file;
			int part,size,offset;
			ss>>file>>to>>part>>offset>>size;
			int ulsock = tcpsocket();
			connect(ulsock,(struct sockaddr*)&(*users)[to].info,sizeof(struct sockaddr_in));
			sprintf(buf,"%s %d %d",file.c_str(),part,size);
			write(ulsock,buf,sizeof(buf));
			ifstream ifs(myid+"/"+file);
			ifs.seekg(offset);
			int prog = 0;
			while(prog<size){
				int toRead = sizeof(buf);
				if(size-prog<toRead)toRead=size-prog;
				ifs.read(buf,toRead);
				int n=ifs.gcount();
				prog+=n;
				write(ulsock,buf,n);
				usleep(50);
			}
			ifs.close();
			close(ulsock);
		}
		else if(com=="size"){
			string file;
			ss>>file;
			ifstream ifs(myid+"/"+file);
			ifs.seekg(0,ios::end);
			int size = ifs.tellg();
			ifs.close();
			write(connfd,&size,sizeof(size));
			close(connfd);
		}
	}
}
void client_handler(int sockfd){
	struct sockaddr_in cliaddr;
	unsigned int size = sizeof(cliaddr);
	getpeername(sockfd,(struct sockaddr*)&cliaddr,&size);
	char buf[2048];
	read(sockfd,buf,2048);
	stringstream ss(buf);
	string id;
	ss>>id;
	(*users)[id]=Client(id,cliaddr);
	string file;
	map<string,bool> files;
	while(ss>>file){
		files[file]=1;
	}
	(*users)[id].files = files;
	sprintf(buf,"Welcome");
	write(sockfd,buf,sizeof(buf));
	while(1){
		bzero(buf,sizeof(buf));
		if(read(sockfd,buf,sizeof(buf))>0){
			stringstream ss(buf);
			string com;
			ss>>com;
			if(com=="list"){
				string r;
				for(auto u:*users){
					r+=u.second.list();
				}
				strcpy(buf,r.c_str());
				write(sockfd,buf,sizeof(buf));
			}
		}
		else break;
	}
	users->erase(id);
	close(sockfd);
	return;
}

int tcpsocket(){
	return socket(AF_INET,SOCK_STREAM,0);
}

void listfile(string path,char *buf){
	DIR* dir = opendir(path.c_str());
	struct dirent* info;
	rewinddir(dir);
	map<string,bool> m;
	while(info = readdir(dir)){
		if(strcmp(info->d_name,".")!=0&&strcmp(info->d_name,"..")&&info->d_type!=DT_DIR!=0){
			m[info->d_name] = 1;
		}
	}
	(*users)[myid].setFiles(m);
	closedir(dir);
}
