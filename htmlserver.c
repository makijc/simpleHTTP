// Simple TCP html server
// COMMENTS
/*
I learned in the last lab period of the year that I was using the wrong version of WSL for the entire course. This was a little
embarassing, but I am glad to have the closure on what was going wrong with my servers. Making a website was very fun. The biggest
problem I encountered was my form was not sending any data back to the server; this was solved by removing a </form> from the first
line of the form, which made the website ignore 90% of the form. Another issue was that my website was not displaying the enitrety of
my html file. This was solved by increasing the size of the buffer that the server was using to send files. My first implementation
was sending files in a 1kB buffer, but by the end of development the buffer was a size of 1MB (necessary for sending mandel images).
Also 404 not sending when the request did not have a .xyz in it.
*/
#include <fcntl.h>
#include <stdio.h>      
#include <stdlib.h>     
#include <unistd.h>  
#include <string.h>	
#include <sys/types.h>  
#include <sys/socket.h> 
#include <sys/stat.h>
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include "mandel.h"


// Max message size
#define MAX_MESSAGE	1000000

// Prototypes
void serviceGET(char* response, size_t* response_len, char* filename);
int mandel_gen(char* input);

/* server main routine */
int main(int argc, char** argv) 
{
	// locals
    char c;
    unsigned int ipv4 = 0;
    unsigned short port = 80; // port for html
	int sock; // socket descriptor
    int pid; // process ID

    // parse options
    while((c = getopt(argc,argv,"p:a:h"))!=-1){
        switch(c)
        {
        case 'p': // port to be used
            port = atoi(optarg);
            break;
        case 'a': // address of server
            printf("IP: %s\n",optarg);
            ipv4 = inet_addr(optarg);
            break;
        case 'h': // help
            printf("HELP GOES HERE\n");
            exit(1);
            break;
        }
    }
	
	// ready to go
	printf("TCP server configuring on port: %d\n",port);
    // for TCP, we want IP protocol domain (PF_INET)
	// and TCP transport type (SOCK_STREAM)
	// no alternate protocol - 0, since we have already specified IP
	
	if ((sock = socket( PF_INET, SOCK_STREAM, 0 )) < 0) 
	{
		perror("Error on socket creation");
		exit(1);
	}
  
  	// lose the pesky "Address already in use" error message
	int yes = 1;

	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) 
	{
		perror("setsockopt");
		exit(1);
	}

	// establish address - this is the server and will
	// only be listening on the specified port
	struct sockaddr_in sock_address;
	
	// address family is AF_INET
	// our IP address is INADDR_ANY (any of our IP addresses)
    // the port number is per default or option above

	sock_address.sin_family = AF_INET;
	sock_address.sin_addr.s_addr = htonl(INADDR_ANY);
    //sock_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    //sock_address.sin_addr.s_addr = inet_addr("10.100.104.199");
	sock_address.sin_port = htons(port);

	// we must now bind the socket descriptor to the address info
	if (bind(sock, (struct sockaddr *) &sock_address, sizeof(sock_address))<0)
	{
		perror("Problem binding");
		exit(-1);
	}
	
	// extra step to TCP - listen on the port for a connection
	// willing to queue 5 connection requests
	if ( listen(sock, 5) < 0 ) 
	{
		perror("Error calling listen()");
		exit(-1);
	}
	
	// go into forever loop and echo whatever message is received
	// to console and back to source
	char* buffer = calloc(MAX_MESSAGE,sizeof(char));
	int bytes_read;
	int echoed;
	int connection;
	
    while (1) {			
		// hang in accept and wait for connection
		if ( (connection = accept(sock, NULL, NULL) ) < 0 ) 
		{
			perror("Error calling accept");
			exit(-1);
		} else {
            printf("Connection received!\n");
        }
		pid = fork();
		if (pid < 0)
			perror("ERROR on fork\n");
		if (pid == 0) {
		// Child Process
		close(sock); // Client is connected to child; child no longer needs
					 // listening socket

		// ready to r/w - another loop - it will be broken when
		// the connection is closed
		while(1)
		{
            //do something
            buffer[0] = '\0'; 	// guarantee a null here to break out on a
								// disconnect

			// read message								
			bytes_read = read(connection,buffer,MAX_MESSAGE-1);
						
			if (bytes_read == 0)
			{	// socket closed
				printf("====Client Disconnected====\n");
				close(connection);
				break;  // break the inner while loop
			} else {
                printf("====Message Received====\n");
                //printf("Message: %s\n",buffer);    
            }

			// make sure buffer has null temrinator
			buffer[bytes_read] = '\0';

            char* tok1 = strtok(buffer, " ");
            char* tok2 = strtok(NULL, " ");
            char* tok3 = strtok(NULL, " \n");
            if(!strcmp(tok1, "GET")){
                // service GET request
                char* response = (char*)malloc(MAX_MESSAGE*2*sizeof(char));
                size_t response_len;
                serviceGET(response, &response_len, tok2);
                write(connection,response,response_len);
            } else if(!strcmp(tok1, "POST")) {
                printf("%s\n",tok3);
            }
        }
        } else close(connection);
    }
}

void serviceGET(char* response, size_t* response_len, char* filename)
{
    char* index = "index.html";
    char* formsub;

    // remove slash from the GET request
    filename = filename+1;
    // separate file request from form submission
    if(strchr(filename,'?') != NULL)
    {
        filename = strtok(filename,"?");
        formsub = strtok(NULL," ");
        if(strlen(formsub) > 0){
            mandel_gen(formsub);
        }
    }

    // was any file requested? If not, serve default page
    printf("requested File: %s\n of size %ld\n",filename, strlen(filename));
    if(strlen(filename)<2){   // check if length is 0 or 1 (newline)
        strcpy(filename,index);
    }

    //open requested file
    int fd = open(filename, O_RDONLY);
    printf("file descriptor: %d\n",fd);

    //determine size
    struct stat st;
    stat(filename, &st);
    int size = st.st_size;
    printf("%s size: %d Bytes\n",filename,size);

    //determine file extension & MIME type
    char* mime;
    if(fd != -1){
    char* tok1 = strtok(filename, ".");
    char* fextension = strtok(NULL, ". \n");
    if(!strcmp(fextension,"html"))
    {
        mime = "text/html";
    } else if(!strcmp(fextension,"png"))
    {
        mime = "image/png";
    } else {
        mime = "application/octet-stream";
    }
    }

    // HEADER //
    char* header = (char*)malloc(MAX_MESSAGE * sizeof(char));
    *response_len = 0;
    // create header; was the file found? If not, send error page
    if(fd == -1 || fd == 0){

        fd = open("notfound.html", O_RDONLY);   // set fd to error page
        stat("notfound.html", &st);
        size = st.st_size;
        snprintf(header, MAX_MESSAGE,
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "404 Not Found\r\n"
        "Content-Length: %d\r\n"
        "\r\n", size);

    } else {

    snprintf(header, MAX_MESSAGE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n", mime, size);

    }
    // add header to response
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);
    //printf("response: %s\n",response);

    size_t bytes_read;
    if(fd != -1)
    {
        while((bytes_read = read(fd, response + *response_len, MAX_MESSAGE - *response_len)) > 0) {
            *response_len += bytes_read;
        }
    } else {

    }
    free(header);
    close(fd);
}

//FCN takes url encoded response: x=&y=&s=&m=&i=&submit=Generate%21
int mandel_gen(char* input){

    // Input Parsing
    char* xstr = strtok(input,"&");
    char* ystr = strtok(NULL,"&");
    char* sstr = strtok(NULL,"&");
    char* mstr = strtok(NULL,"&");
    char* istr = strtok(NULL,"&");
    double x = strtod(xstr+2,NULL); 
    double y = strtod(ystr+2,NULL);
    double s = strtod(sstr+2,NULL);
    int m = atoi(mstr+2);
    int i = atoi(istr+2);
    printf("====Mandel Inputs====\n");
    printf("x = %f\ni = %f\n",x,y);
    printf("s = %f\nm = %d\n",s,m);
    printf("i = %d\n",i);
    
    mandelmain(x,y,s,m,i);
}