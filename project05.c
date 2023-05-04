#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8148

#define MAX_RESPONSE_LEN 4096
#define MAX_HTTP_REQ_LEN 2048

void send_response(int sockfd, const char *status, const char *content_type,
		   const char *body)
{
	char response[MAX_RESPONSE_LEN];
	snprintf(response, sizeof(response),
		"HTTP/1.1 %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s",
		status, content_type, strlen(body), body);
	send(sockfd, response, strlen(response), 0);
}

int main(int argc, char **argv)
{
	/* Creating TCP socket */
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("Failed to create socket");
		exit(-1);
	}

	/* Creating sockaddr_in for server address */
	struct sockaddr_in servaddr = {0};
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);


	int optval = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval,
		       sizeof(optval)) < 0) {
		perror("setsockopt failed...\n");
		exit(-1);
	}

	/* Binding socket to the port */
	if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) {
		perror("Failed to bind socket");
		exit(-1);
	}

	/* Listening for TCP connections */
	if (listen(sockfd, 10) != 0) {
		perror("Failed to listen for connections");
		exit(-1);
	}

	printf("Server listening on port %d...\n", ntohs(servaddr.sin_port));

	/* Receiving & handling the message */
	struct pollfd fds[1];
	fds[0].fd = sockfd;
	fds[0].events = POLLIN;

	while (1) {
		int ret = poll(fds, 1, -1);
		if (ret < 0) {
			perror("poll() error");
			exit(1);
		}

		if (fds[0].revents & POLLIN) {
			struct sockaddr_in cliaddr;
			socklen_t clilen = sizeof(cliaddr);
			int connfd = accept(sockfd,
					    (struct sockaddr *) &cliaddr,
					    &clilen);

			char http_req[MAX_HTTP_REQ_LEN];
			memset(http_req, 0, MAX_HTTP_REQ_LEN);
			read(connfd, http_req, MAX_HTTP_REQ_LEN);
			printf("%s\n", http_req); /* debug */
			if (strstr(http_req, "GET /"))
				send_response(connfd, "200 OK", "text/html", "<!DOCTYPE html>\n<html>\n  <body>\n    Hello CS 221\n  </body>\n</html>\n");
			else
				send_response(connfd, "404 Not Found", "text/html", "<!DOCTYPE html>\n<html>\n  <body>\n    Not found\n  </body>\n</html>\n");

			close(connfd);
		}
	}

	close(sockfd);

	return 0;
}
