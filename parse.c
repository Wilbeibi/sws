#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net.h"
#include "parse.h"
#include "response.h"
#include "requests.h"

static void
rd_timeout(int sig) {
    // response to client of 408?
    // log warn: read timeout
    err_response(_sock, 408);
    exit(0);
}

static void
wt_timeout(int sig) {
    // log warn: write timeout
    exit(0);
}

void init_req(Req_info * req) 
{
    req->status = 200;
	req->contLen = 0;
    req->method = NOT_IMPLEMENTED;
    req->cgi = NO_CGI;
}

/**
 * Full-Request = Request-Line 
 *                *( General-Header
 *                 | Request-Header
 *                 | Entity-Header )
 *                CRLF
 *                [ Entity-Body ]
 */
static int 
read_req_line(int sock, Req_info *req, char *buf)
{
    int ret=0;	
	while(1) {
		if ((ret=Readline(sock, buf))==-1) {
	        req->status=500;
	        break;
	    } else if (strcmp(buf,"\r\n") == 0){
			continue;
		} else if(ret<2 || buf[ret-2] !='\r'|| ret > MAXBUF) {
			req->status=400;
	         break;
	    } else
			break;
	}
	if (req->status != 200)
		return -1;
		
    return ret;
}

/**
 * read all rest content, including headers and entity-body, it should be parted
 * into read_headers and read_body...but we read headeers only now.
 */
static int
read_rest(int sock, Req_info *req, char *buf)
{
    int ret;
    int tot=0;
	char * tmp = buf;
    while ((ret=Readline(sock, tmp))) {
        if (ret==-1) {
            req->status=500;
            return ret;
        }
        if (ret>=MAXBUF) {
            req->status=400;
            return ret;
        }
        tmp+=ret;
        tot+=ret;
        if (tot>=MAXBUF) {
            req->status=400;
            return ret;
        }
		if (tmp == (buf+2) && strcmp(buf,"\r\n") == 0)
			break;
		if (strcmp(tmp-4,"\r\n\r\n") == 0)
			break;
    }
    return tot;
}
/*
void process_path(char * uri)
{
    // path convertion, first tokenize, then combine up 
    char buf[256];
    char temp[256];
    char *bufp=buf;
    int ind=0;
    int len=strlen(uri);
    buf[0]='/';
    buf[1]=0;
    for (int i=0; i<len; i++) {
        while (i<len && uri[i]=='/') i++; 
        while (i<len && uri[i]!='/') temp[ind++]=uri[i++];
        temp[ind]=0;
        if (temp[0]==0)
            continue;
        if (strcmp(temp, "..")!=0) {
            *bufp='/';
            bufp++;
            for (int j=0; j<ind; j++) {
                *bufp=temp[j];
                bufp++;
            }
            *bufp=0;
        } else {
            while (*bufp!='/') bufp--;
            if (bufp==buf) {
                *bufp='/';
                *(bufp+1)=0;
            } else
                *bufp=0;
        }
        ind=0;
    }
    strcpy(uri, buf);
}
*/
void process_path(char * dir)
{
    char * tmp, *tmp2;
    tmp = dir;
    while ((tmp = strstr(tmp, "/..")) != NULL) {
      if ( *(tmp+3) == '/' || *(tmp+3) == '\0') {
        if (tmp == dir) {
          strncpy(dir,"/\0",2);
          break;
        }
        else if (tmp > dir) {
           tmp2 = tmp-1;
          while (tmp2>=dir) { // use tmp2 to find the slash before /../ 
            if (*tmp2 == '/')
              break;
            tmp2 --;
          }
          tmp = tmp+3;
          while(*tmp != '\0') {
            (*tmp2++) = (*tmp++);
          }
          *tmp2 = '\0';
          if (*dir == '\0')
            strncpy(dir,"/\0",2);
          tmp = dir;
        }
      }
      else 
        continue;
    }
}


/**
 * We only need to implement [ path ], at this point, uri doesn't contain SP
 * Request-URI      = abs_path
 * abs_path         = "/" rel_path
 * rel_path         = [ path ] [ ";" params ] [ "?" query ]
 * path             = fsegment *( "/" segment )
 * fsegment         = 1*pchar
 * segment          = *pchar
 */
int parse_uri(Req_info * req, Arg_t *optInfo)
{
    char * tmp = req->uri;
    char usr[256];
    char rest[256];
    int i;
	req->cgi=NO_CGI;
	printf("original uri:%s\n",req->uri);
	/* http://babla. should also be valid
    if (req->uri[0] != '/') {
        req->status = 404;
        return -1;
    }
	*/
	/* According to sws man page, request for user home should start with '~'  */
    if (strncmp(req->uri,"/~",2) == 0) {
        tmp += 2;
        i = 0;
        while (*tmp != '/' ) {
		/* there must be a slash after the user name, otherwise it's invalid*/
			if ( *tmp == '\0') {
				 req->status=404;
		            return -1;	
			}
            usr[i++] = *tmp++;
        }
        usr[i]='\0';
		
        /* if user not exist, return 404 */
        struct passwd *pwd;
        if ((pwd=getpwnam(usr))==NULL) {
            req->status=404;
            return -1;
        }      
        strncpy(rest,tmp+1,256);
		/* prevent spoofing */
		printf("processing:%s\n",rest);
		process_path(rest);		
		printf("afer:%s\n",rest);
		#ifdef __APPLE__
			sprintf(req->uri,"Users/%s/Desktop/%s",usr,rest);
		#else	
			sprintf(req->uri,"/home/%s/sws/%s",usr,rest);
		#endif
        
    }
    else if (strncmp(req->uri,"/cgi-bin/",9) == 0) {
        req->cgi=DO_CGI;
        tmp = req->uri;
        tmp += 9;
		strncpy(rest,tmp,256);
		
		if (optInfo->cgiDir != NULL) {
			printf("processing:%s\n",rest);
			process_path(rest);
			printf("afer:%s\n",rest);
        	sprintf(req->uri,"%s%s",optInfo->cgiDir,rest);	
		}		
		else {
			printf("processing:%s\n",req->uri);
			process_path(req->uri);
		}	
    }
	else {
		char * tmp2 = strdup(req->uri);
		printf("processing:%s\n",tmp2);
		process_path(tmp2);
		printf("afer:%s\n",tmp2);
		sprintf(req->uri,"%s%s",optInfo->dir, (*tmp2=='/') ? (tmp2+1): tmp2 );
		free(tmp2);
	}	
    return 0;
}

/**
 * Note method is case-sensitive
 * SP               = <space>
 * CRLF             = <'\r\n'>
 * token            = 1*<any CHAR except CTLs or tspecials>
 *
 * Request-line     = Method SP Request-URI SP HTTP-Version CRLF
 * HTTP-Version     = "HTTP/" 1*DIGIT "." 1*DIGIT
 * Method           = "GET" | "POST" | "HEAD" | extension-method
 * extension-method = token
 */
int parse_req_line(char * buf, Req_info * req, Arg_t *optInfo)
{    
    char method[10], version[20];
    
    bzero(method,10);
    bzero(req->uri,256);
    bzero(version,20);
    sscanf(buf, "%s %s %s", method, req->uri, version);

    if (strcmp(method, "GET") == 0)
        req->method=GET;
    else if (strcmp(method, "POST") == 0)
        req->method=POST;
    else if (strcmp(method, "HEAD") == 0) {
		req->method=HEAD;
		_head_response = 1;
	}
    else {
        req->status=501;
        return -1;
    }

    if (version[0]==0) {
        // version is not specified, if is GET, validate it as HTTP/0.9 Simple request
		if ((req->method == GET) && req->uri[0] != '\0')
			_simple_response = 1;
		else {
			req->status=400;
	        return -1;
		}
    } else {
	/*
        char *http=strstr(version, "HTTP/");
        if (http==NULL || http!=version) {
            req->status=400;
            return -1;
        }
        http+=5;
        int major=-1;
        int minor=-1;
        sscanf(http, "%d.%d", &major, &minor);
        if ((major==1 && minor==0)
            || (major==0 && minor==9)) {
            return 0;
        }
        req->status=505;
        return -1;
*/
		
    }

    if (req->uri[0]==0) {
        req->status=400;
        return -1;
    }
    
    return 0;
}

void read_sock(int sock, Req_info *req, Arg_t *optInfo)
{
    _sock=sock;
    signal(SIGALRM, rd_timeout);
    alarm(READ_TIMEOUT);

    int ret;
    char buf[MAXBUF];
    
    init_req(req);

    /* read first line*/
    ret=read_req_line(sock,req,buf);
	strncpy(req->fstLine,buf,strlen(buf)+1);
	get_timestamp(req->recvTime);
    if (ret==-1) {
        err_response(sock, req->status);
        return;
    }
   
    ret=parse_req_line(buf,req,optInfo);
    if (ret==-1) {
        err_response(sock, req->status);
        return;
    }

    ret=parse_uri(req, optInfo);
    if (ret==-1) {
        err_response(sock, req->status);
        return;
    }

    /**
     * at this point, buf only contains request-line and the position is at
     * buf[0] because we didn't do buf+=w+1. so read_rest will override
     * request-line, which is acceptable here.
     */
    bzero(buf, MAXBUF);
    ret=read_rest(sock,req,buf);
    if (ret==-1) {
        err_response(sock, req->status);
        return;
    }
	//printf("rest:%s\n",buf);
	//err_response(sock, req->status);
	printf("uri:%s\n",req->uri);
    signal(SIGALRM, wt_timeout);
    alarm(0);
    alarm(WRITE_TIMEOUT);
    serve_request(sock,req);
    return;
	
}
