#include "project05.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#define PORT "8148"

#define MAX_HTTP_REQ_LEN 2048
#define MAX_RESPONSE_LEN 28672

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

	strncpy(fpath, end, MAX_FILE_PATH_LEN);

	return 0;
}

int get_content_type(char *fpath)
{
	char *dot = strrchr(fpath, '.');
	if (dot == NULL) {
		return -1;
	}

	if (!strcmp(dot + 1, "html"))
		return 0;
	else if (!strcmp(dot + 1, "css") || !strcmp(dot + 1, "map"))
		return 1;
	else if (!strcmp(dot + 1, "png"))
		return 2;
	else if (!strcmp(dot + 1, "jpg"))
		return 3;
	else if (!strcmp(dot + 1, "ico"))
		return 4;
	else
		return -1;
}

char *get_content(FILE *fp, char *fpath, size_t *file_sz)
{
	char *content_buf = 0;

	fseek (fp, 0, SEEK_END);
	*file_sz = ftell(fp);
	fseek (fp, 0, SEEK_SET);
	content_buf = malloc(*file_sz);
	if (!content_buf)
		return NULL;
	fread(content_buf, 1, *file_sz, fp);

	return content_buf;
}

void send_response(int sockfd, char *status, char *content_type, char *body, size_t file_sz)
{
	char header[MAX_RESPONSE_LEN];
	snprintf(header, sizeof(header),
		"HTTP/1.1 %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"\r\n",
		status, content_type, file_sz);

	send(sockfd, header, strlen(header), 0);
	send(sockfd, body, MAX_RESPONSE_LEN, 0);
}

int main(int argc, char **argv)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *results;
	int rc = getaddrinfo(NULL, PORT, &hints, &results);
	if (rc != 0) {
		perror("getaddrinfo failed");
	}

	int fd;
	struct addrinfo *r;
	for (r = results; r != NULL; r = r->ai_next) {
		fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (fd == -1)
			continue;  /* failed; try next addrinfo */

		int en = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(int)) == -1) {
			perror("setsockopt reuseaddr");
			exit(0);
		}

		if (ioctl(fd, FIONBIO, (char*) &en) == -1) {
			perror("ioctl");
			exit(0);
		}

		if (bind(fd, r->ai_addr, r->ai_addrlen) == 0) {
			break;  /* success */
		} else {
			perror("bind");
			exit(0);
		}

		close(fd);  /* failed; try next addrinfo */
		fd = -1;
	}

	if (listen(fd, SOMAXCONN) == -1) {
		perror("listen");
	}

	while (1) {
		int new_fd;
		while ((new_fd = accept(fd, NULL, NULL)) == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				/* non-blocking socket, no connections pending */
				/* wait for 1 millisecond and try again */
				usleep(1000);
				continue;
			} else {
				perror("accept failed");
				exit(1);
			}
		}

		struct pollfd pfd[1];
		pfd[0].fd = new_fd;
		pfd[0].events = POLLIN;

		int res = poll(pfd, 1, 5000);
		if (res == -1) {
			perror("poll failed");
			exit(0);
		} else if (res == 0) {
			printf("poll timed out\n");
			exit(0);
		}

		struct request *request = malloc(sizeof(struct request));
		memset(request, 0, sizeof(struct request));
		struct response *response = malloc(sizeof(struct response));
		memset(response, 0, sizeof(struct response));

		char http_req[MAX_HTTP_REQ_LEN + 1];
		memset(http_req, 0, MAX_HTTP_REQ_LEN + 1);
		read(new_fd, http_req, MAX_HTTP_REQ_LEN);
		printf("%s\n", http_req); /* debug */

		parse_req_to_file_path(request->uri, http_req);
		printf("uri: %s\n", request->uri); /* debug */

		strncpy(request->path, "www/cs221.cs.usfca.edu", MAX_FILE_PATH_LEN);
		strncat(request->path, request->uri, MAX_FILE_PATH_LEN);
		if (!strcmp(request->uri, "/"))
			strncat(request->path, "index.html", MAX_FILE_PATH_LEN);
		printf("file path: %s\n", request->path); /* debug */

		switch (get_content_type(request->path)) {
			case 0:
				strncpy(request->content_type, "text/html", MAX_CONTENT_TYPE_LEN);
				break;
			case 1:
				strncpy(request->content_type, "text/css", MAX_CONTENT_TYPE_LEN);
				break;
			case 2:
				strncpy(request->content_type, "image/png", MAX_CONTENT_TYPE_LEN);
				break;
			case 3:
				strncpy(request->content_type, "image/jpeg", MAX_CONTENT_TYPE_LEN);
				break;
			case 4:
				strncpy(request->content_type, "image/vnd.microsoft.icon", MAX_CONTENT_TYPE_LEN);
				break;
			default:
				strncpy(request->content_type, "application/octet-stream", MAX_CONTENT_TYPE_LEN);
				break;
		}
		printf("content type: %s\n\n\n\n", request->content_type); /* debug */

		size_t file_sz;
		FILE *fp = fopen(request->path, "r");
		if (!fp) {
			strncpy(response->status, "404 Not Found", MAX_STATUS_LEN);
			response->content = "<!DOCTYPE html>\n<html>\n  <body>\n    404 Not found\n  </body>\n</html>\n";
			send_response(new_fd, response->status, "text/html", response->content, (size_t)strlen(response->content));

			free(request);
			free(response);
			continue;
		}
		response->content = get_content(fp, request->path, &file_sz);
		fclose(fp);

		send_response(new_fd, "200 OK", request->content_type, response->content, file_sz);
		free(response->content);

		free(request);
		free(response);

		close(new_fd);
	}

	close(fd);
	freeaddrinfo(results);
}
