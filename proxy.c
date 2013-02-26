/*
 * proxy.c - A Simple Sequential Web proxy
 *
 * Course Name: 14:332:456-Network Centric Programming
 * Assignment 2
 * Student Name: Greg Paton
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>

#include "csapp.h"



/*
 * Function prototypes
 */
void err_exit() {
  perror("proxy");
  exit(1);
}


void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

char* readLine(int clientfd, char* line, int maxlen);
int tokenizeRequest(char *request, char **tokens);
int writeToLog(char *string, int size);
int parseURI(char *browserRequest, char *domainName, char *proxyRequest, char *serverPort, int maxLen);
/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    int i; /*universal loop counter*/
    /*Check arguments*/
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }
    
/***************proxy acts as server: receives request from browser***************/
    printf("proxy starting...\nto exit type \"exit\" into the browser\n");
    int port = atoi(argv[1]);
    
    int clientSock;
    if((clientSock=socket(AF_INET, SOCK_STREAM, 0)) < 0)    {
        perror("socket");
        err_exit();
    }

    struct sockaddr_in clientAddr;
    bzero(&clientAddr, sizeof(clientAddr)); /*set entire struct to zeros*/  
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = htons(INADDR_ANY);
    clientAddr.sin_port = htons(port); /*host to network short -- byte order*/
                                       /*converts byte order to allow communication between machines with different byte ordering*/
    if(bind(clientSock, (struct sockaddr*) &clientAddr, sizeof(clientAddr)) < 0)    {
        perror("bind");
        err_exit();
    }
  
    if(listen(clientSock, LISTENQ) < 0) {
        perror("listen");
        err_exit();
    }

	/* Declare dynamically allocated memory*/
	/*declare and initialize 2-dimensional array for tokens*/
	char **tokens = (char**) malloc (3 * sizeof(char*));
	i = 0;
	for(i = 0; i < 3; ++i)
		tokens[i] = (char*) malloc (100 * sizeof(char));
	char serverDomain[100];	
	char serverURL[5000];
	char serverPort[5];
        socklen_t len;
        len = sizeof(clientAddr);

    /****************************LOOP*************************************/
    while(1)	{
        int clientfd, serverfd;
        //if((clientfd = accept(clientSock, 0, 0)) < 0)	{	
        if((clientfd = accept(clientSock, (struct sockaddr*) &clientAddr, &len)) < 0)	{	
            perror("accept");
            err_exit();
        }

        /*get HTTP header and body of request*/
        char httpHeader[5000];
        bzero(httpHeader, sizeof(httpHeader));
        readLine(clientfd, httpHeader, sizeof(httpHeader));
        char httpReq[10000];
        bzero(httpReq, sizeof(httpReq));
        read(clientfd, httpReq, sizeof(httpReq));
    
        /*exit*/
        if(strstr(httpHeader, "GET http://exit/ HTTP/1.1") != NULL)  {
            printf("exiting...\n");
            char header[]="HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
            char exitMessage[500];
            sprintf(exitMessage, "%s\r\n<html><body><h1 style=\"color:red;\">Goodbye</h1></body></html>", header);
            if(send(clientfd, exitMessage, strlen(exitMessage), 0) < 0)
                err_exit();
            close(clientfd);
            close(serverfd);
            break;
        }
    
        /*tokenize HTTP request*/
        if(tokenizeRequest(httpHeader, tokens) < 0)		{	
            perror("tokenizing request");
            err_exit();
        }
	
        /*parse URI*/    
        int serverPortNum;
        if((serverPortNum = parseURI(tokens[1], serverDomain, serverURL, serverPort, 5000)) < 0)		{	
            perror("parsing URI");
            err_exit();
        }
        
	
        /***************proxy acts as client: sends request to web server***************/
        //int serverfd;

        struct hostent *hp;
        struct sockaddr_in serveraddr;
	
        if((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)	{	
            perror("declaring server socket");
            err_exit();
        }
	
        if((hp = gethostbyname(serverDomain)) == NULL)	{	
            herror("gethostbyname");
            err_exit();
        }
	
        bzero((char *) &serveraddr, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length);  
        serveraddr.sin_port = htons(serverPortNum);
	
        if(connect(serverfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)	{
            perror("connect");
            err_exit();
        }
	
        char message[10000];
        bzero(message, sizeof(message));
        char fromWeb[1];
        bzero(fromWeb, sizeof(fromWeb));
           
        /*creat HTTP request and send to web*/
        sprintf(message, "GET %s HTTP/1.1\r\n%s\r\n\r\n", serverURL, httpReq); 
        send(serverfd, message, strlen(message), 0);
	  
        int nread = 1;
        int requestSize = 0;
        while(nread > 0)	{
            if((nread = recv(serverfd, fromWeb, sizeof(fromWeb), 0)) < 0)	{
                err_exit();
            }	
            if(nread > 0)	{	
                if(send(clientfd, fromWeb, sizeof(fromWeb), 0) <= 0)
                    err_exit();
            }
            else 
                break;
            requestSize += nread;
        }

        char logString[200];
        format_log_entry(logString, &clientAddr, serverDomain, requestSize);
        if(writeToLog(logString, strlen(logString)) < 0)        {
            perror("write to log");
            err_exit();
        }

        close(clientfd);
        close(serverfd);
    }
    
    close(clientSock);
	/*not sure how to deallocate a 2-dimensional array. the loop caused a free(): invalid pointer error*/
    /*for(i = 0; i < 3; ++i)
		free(tokens[i]);*/
	free(tokens);
	
    printf("proxy closed\n");
    exit(0);
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}


char* readLine(int clientfd, char* line, int maxlen) {
  // read one character at a time until we encounter \n
  char c='a';
  char* lineptr=line;
  int nread=0;     

  do {  
    if(read(clientfd, &c, sizeof(c)) < 0) {
      if(errno==EINTR)
	continue;
      err_exit();
    }
    nread++;
    *lineptr++=c;    
  } while(c != '\n' && nread<maxlen-1);

  if(c=='\n')
    *(lineptr-1)=0; // overwrite \n if present
  else
    *lineptr=0; // do not overwrite if different character
  return line;
}


/*tokenizes HTTP request as well as verifies the request. returns 0 if the request is valid and -1 if the request is invalid*/
int tokenizeRequest(char *request, char **tokens)	{
	tokens[0] = strtok(request, " ");
	tokens[1] = strtok((char*)NULL, " ");
	tokens[2] = strtok((char*)NULL, "\r\n");	
	if(strcmp(tokens[0], "GET"))    {
        char error[100];
        sprintf(error, "%s: not a GET request", tokens[0]);
        perror(error);
		return -1;
    }
	if(strcmp(tokens[2], "HTTP/1.1"))   {
        perror("not a HTTP/1.1 request");
		return -1;
    }
	return 0;
}


int writeToLog(char *string, int size)	{
	FILE *fileName;
	if((fileName = fopen("proxy.log", "a")) == NULL)	{
		perror("write to log: fopen");
		return -1;
	}
	if(fwrite(string, 1, size, fileName) < size)	{
		perror("write to log: fwrite: string");
		return -1;
	}
    if(fwrite("\r\n", 1, 2, fileName) < 2)	{
		perror("write to log: fwrite: return char");
		return -1;
	}
	if(fclose(fileName) == EOF)	{
		perror("write to log: fclose");
		return -1;
	}
    return 0;
}


int parseURI(char *browserRequest, char *domainName, char *proxyRequest, char *serverPort, int maxLen)	{
	char curr;
	int port;
	char portBuf[5];
    portBuf[0] = 'A';
	int i = 0;
	int j = 0;
	int k = 0;
    int l = 0;
	int slashCount = 0;
	int colonFound = 0;
	while((curr = browserRequest[i]) != '\0')	{
		if(curr == '/')	
			++slashCount;
		else if((slashCount == 2) && (curr == ':'))
			colonFound = 1;
        else if((slashCount == 2) && !colonFound)   {
            domainName[l] = curr;
            ++l;
        }
		else if((slashCount == 2) && colonFound)	{
			if(j == 5)
				return -1;
			portBuf[j] = curr;
			++j;
		}
		if(slashCount >= 3)	{
			if(k == (maxLen-1))
				return -1;
			proxyRequest[k] = curr;
			++k;
		}
		++i;
	}
	domainName[l] = '\0';
	proxyRequest[k] = '\0';
	portBuf[j] = '\0';
    if(portBuf[0] == 'A')
        serverPort = "80";
    else
        serverPort = portBuf;
	if((port = atoi(portBuf)) == 0)
		return 80;
	else
		return port;
}
