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
using namespace std;

bool hasfile(const char* path,const char* tar){
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

int main(int argc, char const *argv[])
{
	string input;

	int cli_fd = socket(AF_INET,SOCK_STREAM,0);
	char buf[2048]={0};

	struct sockaddr_in svr_addr;
	bzero(&svr_addr,sizeof(svr_addr));
	svr_addr.sin_family = AF_INET;
	svr_addr.sin_addr.s_addr = inet_addr(argv[1]);
	svr_addr.sin_port = htons(atoi(argv[2]));
	if(connect(cli_fd,(struct sockaddr*)&svr_addr,sizeof(svr_addr))<0){
		std::cout<<"Connect failed.\n";
		return 0;
	}

	while(1){
		bzero(buf,sizeof(buf));
		cin>>input;
		if(input=="dl"){
			string filename;
			cin>>filename;
			sprintf(buf,"dl %s",filename.c_str());
			write(cli_fd,buf,sizeof(buf));
			bzero(buf,sizeof(buf));
			read(cli_fd,buf,sizeof(buf));
			cout<<buf<<"\n";
			int size=0;
			sscanf(buf,"Download start, size: %d",&size);
			if(size==0)continue;
			FILE* f = fopen(filename.c_str(),"wb");
			while(ftell(f)<size){
				bzero(buf,sizeof(buf));
				int len = read(cli_fd,buf,sizeof(buf));
				fwrite(buf,sizeof(char),len,f);
			}
			bzero(buf,sizeof(buf));
			read(cli_fd,buf,sizeof(buf));
			cout<<buf<<"\n";
			fclose(f);
		}
		else if(input=="ls"){
			sprintf(buf,"ls");
			write(cli_fd,buf,sizeof(buf));
			bzero(buf,sizeof(buf));
			read(cli_fd,buf,sizeof(buf));
			cout<<buf<<"\n";
		}
		else if(input=="cd"){
			string target;
			cin>>target;
			sprintf(buf,"cd %s",target.c_str());
			write(cli_fd,buf,sizeof(buf));
			bzero(buf,sizeof(buf));
			read(cli_fd,buf,sizeof(buf));
			cout<<"Current folder: "<<buf<<"\n";
		}
		else if(input=="ul"){
			char filename[256];
			cin>>filename;
			if(hasfile(getcwd(0,0),filename)){
				FILE* f = fopen(filename,"rb");
				fseek(f,0,SEEK_END);
				int size = ftell(f);
				rewind(f);
				bzero(buf,sizeof(buf));
				sprintf(buf,"ul %s %d",filename,size);
				write(cli_fd,buf,sizeof(buf));
				bool success = 1;
				while(ftell(f)<size){
					bzero(buf,sizeof(buf));
					int len = fread(buf,sizeof(char),sizeof(buf),f);
					if(write(cli_fd,buf,len)<0){
						cout<<"Upload failed.\n";
					}
				}
				fclose(f);
				usleep(500000);
				if(success)cout<<"Upload success!\n";
			}
			else{
				cout<<"No such file\n";
			}
		}
		else if(input=="mkdir"){
			char dirname[256];
			cin>>dirname;
			bzero(buf,sizeof(buf));
			sprintf(buf,"mkdir %s",dirname);
			write(cli_fd,buf,sizeof(buf));
		}
		else if(input=="exit"){
			close(cli_fd);
			break;
		}
	}

	return 0;
}
