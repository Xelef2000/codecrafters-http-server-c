#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h> 
#include <limits.h>

#define BUFFSIZE 4096
#define RESP_HEADER_SIZE 47




struct request
{
	char* method;
	char* path;
	char* host;
	char* user_agent;
	char* accept;
};

struct response
{
	char* status;
	char* content_type;
	char* body;
};

void print_request(const struct request* req) {
    printf("Method: %s\n", req->method ? req->method : "(not specified)");
    printf("Path: %s\n", req->path ? req->path : "(not specified)");
    printf("Host: %s\n", req->host ? req->host : "(not specified)");
    printf("User-Agent: %s\n", req->user_agent ? req->user_agent : "(not specified)");
    printf("Accept: %s\n", req->accept ? req->accept : "(not specified)");
}

struct response* not_found(struct request* rq){
	struct response* rsp = malloc(sizeof(struct response));
	rsp->body = "";
	rsp->status = "404 Not Found";
	rsp->content_type =  "text/plain";

	return rsp;		
}



struct response* accept_root(struct request* rq){
	if(strcmp(rq->path, "/") != 0 || strcmp(rq->method, "GET") != 0){
		return NULL;
	}

	
	struct response* rsp = malloc(sizeof(struct response));
	rsp->body = "";
	rsp->status = "200 OK";
	rsp->content_type =  "text/plain";

	return rsp;		
}

struct response* accept_echo(struct request* rq){
	if(strstr(rq->path,"/echo") != rq->path || strcmp(rq->method, "GET") != 0){
		return NULL;
	}

	
	struct response* rsp = malloc(sizeof(struct response));
	rsp->body = strstr(rq->path, "echo")+5;
	rsp->status = "200 OK";
	rsp->content_type =  "text/plain";

	return rsp;		
}



struct response* accept_user_agent(struct request* rq){
	if(strstr(rq->path,"/user-agent") != rq->path || strcmp(rq->method, "GET") != 0){
		return NULL;
	}
	
	struct response* rsp = malloc(sizeof(struct response));
	rsp->body = rq->user_agent;
	rsp->status = "200 OK";
	rsp->content_type =  "text/plain";

	return rsp;		
}
struct response* (*path_funcs[])(struct request*) = {accept_root, accept_echo, accept_user_agent, not_found, NULL};



//bc i'm lazy
void print(char* s){
	printf("%s\n",s);
}

int numPlaces (int n) {
    int r = 1;
    if (n < 0) n = (n == INT_MIN) ? INT_MAX: -n;
    while (n > 9) {
        n /= 10;
        r++;
    }
    return r;
}

int send_response(int socket, struct response* rsp){
	int rsp_size = RESP_HEADER_SIZE + strlen(rsp->body) + strlen(rsp->content_type) + numPlaces(strlen(rsp->body)) + strlen(rsp->status);
	char response[rsp_size];
	sprintf(response, "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n%s",rsp->status, rsp->content_type,strlen(rsp->body),rsp->body);
	// printf(response);
	return send(socket, response, strlen(response), 0);
}



int parse_request_new(struct request* request, char* client_message) {
    // Initializing fields to NULL to handle potential errors
    request->method = NULL;
    request->path = NULL;
    request->host = NULL;
    request->user_agent = NULL;
    request->accept = NULL;

    char* line = strtok(client_message, "\r\n");

    // Parse request line
    if (line) {
        sscanf(line, "%ms %ms %*s", &(request->method), &(request->path));
    } else {
        return -1; // Invalid request format
    }

    // Parse header lines
    while ((line = strtok(NULL, "\r\n"))) {
        if (strncmp(line, "Host:", 5) == 0) {
            sscanf(line, "Host: %ms", &(request->host));
        } else if (strncmp(line, "User-Agent:", 11) == 0) {
            sscanf(line, "User-Agent: %ms", &(request->user_agent));
        } else if (strncmp(line, "Accept:", 7) == 0) {
            sscanf(line, "Accept: %ms", &(request->accept));
        }
        // Add more header fields as needed
    }

    return 0; // Successful parsing
}


void *connection_handler(void *socket_desc)
{
	// printf("\n");
    int sock = *(int*)socket_desc;
    int read_size;
    char *message , client_message[BUFFSIZE];

	ssize_t bytes_received = recv(sock, client_message, sizeof(client_message) - 1, 0);
	if (bytes_received < 1) {
		goto terminate_connection;
	}	
	// printf(client_message);

	struct request rq;
	parse_request_new(&rq, client_message);

	print_request(&rq);

	int index = 0;
	struct response* rsp = NULL;
	while(path_funcs[index] != NULL && rsp ==NULL){
		rsp =  path_funcs[index](&rq);
		index++;
	}
	
	send_response(sock,rsp);

	

	

	// const char *response = (strcmp(path, "/") == 0)
    // ? "HTTP/1.1 200 OK\r\n\r\n"
    // : "HTTP/1.1 404 Not Found\r\n\r\n";

	// send(sock, response, strlen(response), 0);

     
    // while( (read_size = recv(sock , client_message , BUFFSIZE , 0)) > 0 )
    // {
	// 	client_message[read_size] = '\0';
	// 	printf("From client: %s \n", client_message);
		
	// 	const char *pong_response = "+PONG\r\n";
   	// 	write(sock, pong_response, strlen(pong_response));
		
		
	// 	memset(client_message, 0, BUFFSIZE);
    // }
     
    if(read_size == 0)
    {
        puts("Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }

	// free(rq.method);
	// free(rq.path);
    
	terminate_connection:
    close(sock);
    return 0;
} 


int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	
	int server_fd, client_addr_len, connfd;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	pthread_t thread_id;
	
	
	while((connfd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len))){
		printf("Client accepted\n");
		if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) &connfd) < 0)
        {
            perror("could not create thread");
            return 1;
        }

		printf("Handler assigned\n");
		pthread_detach(thread_id);
	}

	if (connfd < 0)
    {
        perror("accept failed");
        return 1;
    }

	
	
	close(server_fd);

	return 0;
}


