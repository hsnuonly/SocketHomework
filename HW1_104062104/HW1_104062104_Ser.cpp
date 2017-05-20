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

using namespace std;

char* rootpath;
bool hasdir(char*,char*);
bool hasfile(char*,char*);
void listfile(char*,char*);

class Client{
public:
	int fd;
	string ip;
	char* cur_path;
	Client(int fd,struct sockaddr_in sock){
		this->fd = fd;
		stringstream ss;
		ss<< inet_ntoa(sock.sin_addr)<<":"<<sock.sin_port;
		ip = ss.str();
		cur_path = rootpath;
	}
	const char* toString(){
		return ip.c_str();
	}
	bool isset(fd_set* set){
		return FD_ISSET(fd,set);
	}
	void part(fd_set* set){
		FD_CLR(fd,set);
	}
	bool chdir(char* path){
		int len = strlen(cur_path);
		if(strcmp(path,"..")==0&&len>1){
			int i;
			for(i=len-1;i>=0;i--){
				if(cur_path[i]=='/'){
					cur_path[i]=0;
					break;
				}
				cur_path[i]=0;
			}
		}
		else if(hasdir(cur_path,path)){
			sprintf(cur_path,"%s/%s",cur_path,path);
		}
	}
};

int main(int argc, char const *argv[])
{
	int listenid,connfd;
	struct sockaddr_in servaddr;
	char buf[2048];

	fd_set tempfds,fds;
	FD_ZERO(&fds);

	listenid = socket(AF_INET,SOCK_STREAM,0);
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[1]));

	bind(listenid,(struct sockaddr*)&servaddr,sizeof(servaddr));

	listen(listenid,10);
	int maxfdp1=listenid+1;
	FD_SET(listenid,&fds);
	vector<Client> cli;
	rootpath = getcwd(0,0);
	while(1){
		tempfds=fds;
		if(select(maxfdp1,&tempfds,NULL,NULL,NULL)<=0)continue;
		if(FD_ISSET(listenid,&tempfds)){
			unsigned int addrlen = sizeof(servaddr);
			connfd = accept(listenid,(struct sockaddr*)&servaddr,&addrlen);					std::cout<<inet_ntoa(servaddr.sin_addr)<<"\t"<<servaddr.sin_port<<"\t"<<"\n";
			FD_SET(connfd,&fds);
			maxfdp1=max(maxfdp1-1,connfd)+1;
			Client newclient(connfd,servaddr);
			cli.push_back(newclient);
		}
		else for(int i=0;i<cli.size();i++){
			if(cli[i].isset(&tempfds)){
				bzero(buf,sizeof(buf));
				if(read(cli[i].fd,buf,sizeof(buf))<=0){
					std::cout <<cli[i].toString()<< " disconnected." << '\n';
					FD_CLR(cli[i].fd,&fds);
					close(cli[i].fd);
					cli.erase(cli.begin()+i);
					i--;
				}else{
					char command[256];
					sscanf(buf,"%s",command);
					cout<<cli[i].toString()<<": "<<buf<<"\n";
					if(strcmp(command,"ls")==0){
						listfile(cli[i].cur_path,buf);
						write(cli[i].fd,buf,sizeof(buf));
					}
					else if(strcmp(command,"dl")==0){
						char filename[256] = {0};
						sscanf(buf,"%s %s",command,filename);
						bzero(buf,sizeof(buf));
						if(hasfile(cli[i].cur_path,filename)){
							char filepath[2048] = {0};
							sprintf(filepath,"%s/%s",cli[i].cur_path,filename);
							fstream f(filepath,ios::in);
							f.seekg(0,ios::end);
							int size = f.tellg();
							f.seekg(0,ios::beg);
							sprintf(buf,"Download start, size: %d",size);
							write(cli[i].fd,buf,sizeof(buf));
							while(f.tellg()<size){
								bzero(buf,sizeof(buf));
								int len = f.read(buf,sizeof(buf)).gcount();
								write(cli[i].fd,buf,len);
							}
							f.close();
							bzero(buf,sizeof(buf));
							usleep(500000);
							sprintf(buf,"Download Success!");
							write(cli[i].fd,buf,sizeof(buf));
						}
						else{
							sprintf(buf,"No such file");
							write(cli[i].fd,buf,sizeof(buf));
						}
					}
					else if(strcmp(command,"cd")==0){
						char tar_path[256];
						sscanf(buf,"%s %s",command,tar_path);
						cli[i].chdir(tar_path);
						bzero(buf,sizeof(buf));
						sprintf(buf,"%s",cli[i].cur_path);
						write(cli[i].fd,buf,sizeof(buf));
					}
					else if(strcmp(command,"ul")==0){
						char filename[256];
						int size;
						sscanf(buf,"%s %s %d",command,filename,&size);
						char path[512];
						sprintf(path,"%s/%s",cli[i].cur_path,filename);
						FILE* f = fopen(path,"wb");
						while(ftell(f)<size){
							bzero(buf,sizeof(buf));
							int len = read(cli[i].fd,buf,sizeof(buf));
							fwrite(buf,sizeof(char),len,f);
						}
						fclose(f);
					}
					else if(strcmp(command,"mkdir")==0){
						char dirname[256];
						char dirpath[512]={0};
						sscanf(buf,"%s %s",command,dirname);
						sprintf(dirpath,"%s/%s",cli[i].cur_path,dirname);
						mkdir(dirpath,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
					}
				}
			}
		}
	}
	return 0;
}

bool hasdir(char* path,char* tar){
	DIR* dir = opendir(path);
	struct dirent* info;
	while(info = readdir(dir)){
		if(strcmp(info->d_name,tar)==0&&info->d_type==DT_DIR){
			closedir(dir);
			return 1;
		}
	}
	closedir(dir);
	return 0;
}
bool hasfile(char* path,char* tar){
	DIR* dir = opendir(path);
	struct dirent* info;
	while(info = readdir(dir)){
		if(strcmp(info->d_name,tar)==0&&info->d_type!=DT_DIR){
			closedir(dir);
			return 1;
		}
	}
	closedir(dir);
	return 0;
}
void listfile(char* path,char *buf){
	bzero(buf,sizeof(buf));
	DIR* dir = opendir(path);
	struct dirent* info;
	while(info = readdir(dir)){
		if(strcmp(info->d_name,".")!=0&&strcmp(info->d_name,"..")!=0&&info->d_type==DT_DIR){
			strcat(buf,info->d_name);
			buf[strlen(buf)]=' ';
		}
	}
	strcat(buf," | ");
	rewinddir(dir);
	while(info = readdir(dir)){
		if(strcmp(info->d_name,".")!=0&&strcmp(info->d_name,"..")&&info->d_type!=DT_DIR!=0){
			strcat(buf,info->d_name);
			buf[strlen(buf)]=' ';
		}
	}
	buf[strlen(buf)]=0;
	closedir(dir);
}
