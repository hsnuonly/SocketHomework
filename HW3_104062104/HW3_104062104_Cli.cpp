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
		ss<<info.sin_port;
		port = ss.str();
		this->info = info;
	}
};

map<string,Client>* users;

void p2p_handler(int sockfd);
int tcpsocket();
void updateList(int sockfd);
void listfile(string path,char *buf);

string myid;
int serverfd;

int main(int argc, char const *argv[]) {
	char buf[2048];
	cout<<"ID: ";
	cin>>myid;
	mkdir(myid.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
	listfile(myid.c_str(),buf);
	users = new map<string,Client>();

	serverfd = tcpsocket();
	struct sockaddr_in svraddr;
	svraddr.sin_family=AF_INET;
	svraddr.sin_port = htons(atoi(argv[2]));
	svraddr.sin_addr.s_addr = inet_addr(argv[1]);

	connect(serverfd,(struct sockaddr*)&svraddr,sizeof(svraddr));
	write(serverfd,buf,2048);
	svraddr.sin_port = htons(atoi(argv[2])+1);
	(*users)["server"] = Client("server",svraddr);

	read(serverfd,buf,sizeof(buf));
	cout<<buf<<"\n";

	updateList(serverfd);

	int p2pfd = tcpsocket();
	svraddr.sin_addr.s_addr = inet_addr(argv[1]);
	svraddr.sin_port = (*users)[myid].info.sin_port;
	bind(p2pfd,(struct sockaddr*)&svraddr,sizeof(svraddr));
	listen(p2pfd,10);

	thread thread_p2p(p2p_handler,p2pfd);

	while(1){
		string com;
		cin>>com;
		if(com=="list"){
			updateList(serverfd);
		}
		else if(com=="upload"){
			updateList(serverfd);
			string to,file;
			cin>>to>>file;
			vector<string> peer;
			for(auto u:*users){
				if(u.first==to)continue;
				if(u.second.files[file])peer.push_back(u.first);
			}

			// get size
			ifstream ifs(myid+"/"+file);
			ifs.seekg(0,ios::end);
			int size = ifs.tellg();
			ifs.close();
			int bpp = size/peer.size();

			// tell dl
			int dlsock = tcpsocket();
			connect(dlsock,(struct sockaddr*)&((*users)[to].info),sizeof(struct sockaddr_in));
			sprintf(buf,"download %s %d",file.c_str(),peer.size());
			write(dlsock,buf,sizeof(buf));
			close(dlsock);

			for(int i=0;i<peer.size();i++){
				int ulsock = tcpsocket();
				connect(ulsock,(struct sockaddr*)&(*users)[peer[i]].info,sizeof(struct sockaddr_in));
				if(i==peer.size()-1)sprintf(buf,"upload %s %s %d %d %d",file.c_str(),to.c_str(),i,i*bpp,size-i*bpp);
				else sprintf(buf,"upload %s %s %d %d %d",file.c_str(),to.c_str(),i,i*bpp,bpp);
				write(ulsock,buf,sizeof(buf));
				close(ulsock);
			}
		}
		else if(com=="download"){
			updateList(serverfd);
			string file;
			cin>>file;
			vector<string> peer;
			for(auto u:*users){
				if(u.first==myid)continue;
				if(u.second.files[file]){
					peer.push_back(u.first);
				}
			}
			// tell dl
			int dlsock = tcpsocket();
			connect(dlsock,(struct sockaddr*)&(*users)[myid].info,sizeof(struct sockaddr_in));
			sprintf(buf,"download %s %d",file.c_str(),peer.size());
			write(dlsock,buf,sizeof(buf));
			close(dlsock);

			//ask
			int asker = tcpsocket();
			connect(asker,(struct sockaddr*)&(*users)[peer[0]].info,sizeof(struct sockaddr_in));
			sprintf(buf,"size %s",file.c_str());
			write(asker,buf,sizeof(buf));
			int size;
			read(asker,&size,sizeof(size));
			close(asker);

			int bpp = size/peer.size();

			for(int i=0;i<peer.size();i++){
				int ulsock = tcpsocket();
				connect(ulsock,(struct sockaddr*)&(*users)[peer[i]].info,sizeof(struct sockaddr_in));
				if(i==peer.size()-1)sprintf(buf,"upload %s %s %d %d %d",file.c_str(),myid.c_str(),i,i*bpp,size-i*bpp);
				else sprintf(buf,"upload %s %s %d %d %d",file.c_str(),myid.c_str(),i,i*bpp,bpp);
				write(ulsock,buf,sizeof(buf));
				close(ulsock);
			}
		}
	}
	close(serverfd);
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
	cout<<"Start downloading "<<file<<" part "<<part<<"\n";
	while(prog<size){
		bzero(buf,sizeof(buf));
		int n = read(sockfd,buf,sizeof(buf));
		prog+=n;
		ofs.write(buf,n);
	}
	ofs.close();
	close(sockfd);
	cout<<"Downloading "<<file<<" part "<<part<<" complete.\n";
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
			updateList(serverfd);
			close(connfd);
			string to,file;
			int part,size,offset;
			ss>>file>>to>>part>>offset>>size;
			cout<<"Start uploading "<<file<<" part "<<part<<"\n";
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
				//cout<<prog<<"\n";
				write(ulsock,buf,n);
				usleep(50);
			}
			ifs.close();
			close(ulsock);
			cout<<"Uploading "<<file<<" part "<<part<<" complete.\n";
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

int tcpsocket(){
	return socket(AF_INET,SOCK_STREAM,0);
}

void listfile(string path,char *buf){
	string temp = path;
	temp+=' ';
	DIR* dir = opendir(path.c_str());
	struct dirent* info;
	rewinddir(dir);
	while(info = readdir(dir)){
		if(strcmp(info->d_name,".")!=0&&strcmp(info->d_name,"..")&&info->d_type!=DT_DIR!=0){
			temp+=info->d_name;
			temp+=" ";
		}
	}
	if(temp==path+" "){
		temp+="null ";
	}
	closedir(dir);
	cout<<temp<<"\n";
	strcpy(buf,temp.c_str());
}
void updateList(int sockfd){
	char buf[2048];
	sprintf(buf,"list");
	write(sockfd,buf,sizeof(buf));
	read(sockfd,buf,sizeof(buf));
	stringstream ss(buf);
	string client;
	cout<<buf<<"\n";
	while(ss>>client){
		cout<<client<<" : \n";
		if(client=="server"){
			string file,a,b;
			ss>>a>>b;
			map<string,bool> m;
			while(ss>>file){
				if(file=="|")break;
				m[file] = 1;
				cout<<"\t"<<file<<"\n";
			}
			(*users)[client].files=m;
		}
		else{
			string file,ip;
			int port;
			ss>>ip>>port;
			map<string,bool> m;
			while(ss>>file){
				if(file=="|")break;
				cout<<"\t"<<file<<"\n";
				m[file] = 1;
			}
			struct sockaddr_in info;
			info.sin_family = AF_INET;
			info.sin_port = port;
			info.sin_addr.s_addr = inet_addr(ip.c_str());
			(*users)[client]=Client(client,info);
			(*users)[client].files = m;
		}
	}
}
