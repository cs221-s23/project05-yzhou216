#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8148

#define MAX_RESPONSE_LEN 28672
#define MAX_HTTP_REQ_LEN 2048
#define MAX_FILE_PATH_LEN 256

/*
 * Original http request will be destroyed by strseq, if the http request needs
 * to be reused later, store it into a temp variable first and restore it after
 * calling parse_req_to_file_path().
 */
int parse_req_to_file_path(char *fpath, char *http_req)
{
	char *method = strsep(&http_req, " ");
	char *end = strsep(&http_req, " ");
	if (!method || !end)
		return 1;

	memset(fpath, 0, MAX_FILE_PATH_LEN + 1);
	strncpy(fpath, end, MAX_FILE_PATH_LEN);

	return 0;
}

char *get_content(FILE *fp, char *fpath)
{
	char *content_buf = 0;
	long file_sz;

	if (fp) {
		fseek (fp, 0, SEEK_END);
		file_sz = ftell(fp);
		fseek (fp, 0, SEEK_SET);
		content_buf = malloc(file_sz);
		if (content_buf) {
			fread(content_buf, 1, file_sz, fp);
		}
		fclose(fp);
	}
	return content_buf;
}

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

			char fpath[MAX_FILE_PATH_LEN + 1];
			char http_req[MAX_HTTP_REQ_LEN];
			memset(http_req, 0, MAX_HTTP_REQ_LEN);
			read(connfd, http_req, MAX_HTTP_REQ_LEN);
			printf("%s\n", http_req); /* debug */

			parse_req_to_file_path(fpath, http_req);
			printf("file path: %s\n", fpath); /* debug */

			char relative_path[MAX_FILE_PATH_LEN] = "www/cs221.cs.usfca.edu";
			strcat(relative_path, fpath);
			if (!strcmp(fpath, "/"))
				strcat(relative_path, "index.html");

			printf("relative path: %s\n\n\n", relative_path); /* debug */

			FILE *fp = fopen(relative_path, "r");
			if (!fp) {
				send_response(connfd, "404 Not Found", "text/html", "<!DOCTYPE html>\n<html>\n  <body>\n    404 Not found\n  </body>\n</html>\n");
				goto not_found_out;
			}
			char *cp = get_content(fp, relative_path);
			send_response(connfd, "200 OK", "text/html", cp);

			free(cp);
			close(connfd);
not_found_out:
		}
	}

	close(sockfd);

	return 0;
}
