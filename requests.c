#include <stdio.h>

#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <libgen.h>
#include <dirent.h>

#ifdef __APPLE__
#include <stdlib.h>
#include <string.h>
#else

#include <bsd/string.h>
#include <bsd/stdlib.h>

#endif


#include "net.h"
#include "parse.h"
#include "response.h"

int serve_static(int fd, Req_info *req, int fsize);
int serve_dir(int fd, Req_info * req);

/* return 0 if success */
int serve_request(int fd, Req_info *req){
	struct stat sbuf;
	if(stat(req->uri, &sbuf) < 0){
		err_response(fd, 404);
		return 1;
	}
	
	if(req->cgi == NO_CGI){		/* static */
		if( (S_ISREG(sbuf.st_mode)) && (S_IRUSR & sbuf.st_mode)) {
			return serve_static(fd, req, sbuf.st_size);
			
		}
		else if ((S_ISDIR(sbuf.st_mode)) && (S_IRUSR & sbuf.st_mode)) {			
			return serve_dir(fd, req);
		}
		else {
			err_response(fd, 403);
			return 1;
		}		
	}
	else {						/* dynamic */
		if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
			err_response(fd, 403);
			return 1;
		}
		/* req' uri*/
		//server_dynamic(fd, req)
	}
	return 0;
}

int serve_dir(int fd, Req_info * req) {
	
	DIR *dp;
	char index[256];
	char buf[MAXBUF];
	char content[MAXBUF];
	char date[256];
	struct stat sbuf;
	struct stat sb;
	
	struct dirent *dirp;
	char last_modified[256];
	char path[256];
	char permission[20];
	if (req->uri[strlen(req->uri)-1]!='/')
		strcat(req->uri,"/");
	sprintf(index, "%sindex.html",req->uri);
	if( !lstat(index, &sbuf)){
		strncpy(req->uri,index,strlen(index)+1);
		return serve_static(fd, req,sbuf.st_size);
	}
	
	if ((dp = opendir(req->uri)) == NULL ) {
		err_response(fd, 403);
		return 1;
	}
	sprintf(content,"<!DOCTYPE><html><head><title>Four0Four sws</title></head><body><h1>Index of %s:</h1><br/><table><tr><th align='left'>Permission:</th><th align='left'>Name:</th><th align='right'>Last_Modified:</th></tr><tr><th colspan='5'><hr></th></tr>",basename(req->uri));	
	
	while ((dirp = readdir(dp)) != NULL ) {
		if (dirp->d_name[0] != '.') {
			sprintf(path,"%s%s",req->uri,dirp->d_name);
			if (stat(path, &sb) == -1) {
				if (lstat(path, &sb) == -1) {
					strcpy(last_modified,"Cannot stat\0");
				}
			} 
			else {
				strmode(sb.st_mode, permission);
				struct tm * tmp;
				tmp = gmtime(&sb.st_mtime);
				char *Wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
				char *Mth[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
				sprintf(last_modified,"%s, %d %s %d %d:%d:%d GMT",Wday[tmp->tm_wday],(tmp->tm_mday),Mth[tmp->tm_mon],(1900+tmp->tm_year),(tmp->tm_hour),(tmp->tm_min),(tmp->tm_sec));	
			}			
			sprintf(content, "%s<tr><td align='left'>%s</td><td align='left'>%s</td><td align='right'>%s</td></tr>",content,permission,dirp->d_name,last_modified);	
		}
	}		
	sprintf(content, "%s<tr><th colspan='5'><hr></th></tr></table><br/><span>Four0Four Server page</span></body></html>",content);	
	closedir(dp);
	
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	get_timestamp(date);
	sprintf(buf, "%sDate: %s\r\n",buf,date);
	sprintf(buf, "%sServer: Four0Four\r\n", buf);
	sprintf(buf, "%sContent-type: text/html\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, (int)strlen(content));
	
	Send(fd, buf, strlen(buf), 0);
	Send(fd, content, strlen(content), 0);
	req->contLen = (int)strlen(content);
	logging(req);
	return 0;
}

int serve_static(int fd, Req_info *req, int fsize){
	char buf[MAXBUF];
	int datafd;
	char *datap;
	char date[256];
	
	if((datafd = open(req->uri, O_RDONLY, 0)) < 0){
		fprintf(stderr, "open error.\n");
		return 1;
	}
	
	if((datap = mmap(0, fsize, PROT_READ, MAP_PRIVATE, datafd, 0)) == MAP_FAILED){
		fprintf(stderr, "open error.\n");
		return 1;
	}
	Close(datafd);
	
	/* Send response headers to client */
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	get_timestamp(date);
	sprintf(buf, "%sDate: %s\r\n",buf,date);
	sprintf(buf, "%sServer: Four0Four\r\n", buf);
	sprintf(buf, "%sContent-type: text/html\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n\r\n", buf,fsize);
	Send(fd, buf, strlen(buf), 0);
	
	/* Send response body to client */
	Send(fd, datap, fsize, 0);	/* fsize == strlen(datap) */
	req->contLen = fsize;
	logging(req);
	munmap(datap, fsize);
	return 0;
}

int serve_dynamic(){
	
	return 0;
}
