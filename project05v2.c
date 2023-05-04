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
	else if (!strcmp(dot + 1, "ico"))
		return 3;
	else
		return -1;
}

char *get_content(FILE *fp, char *fpath)
{
	char *content_buf = 0;
	long file_sz;

	fseek (fp, 0, SEEK_END);
	file_sz = ftell(fp);
	fseek (fp, 0, SEEK_SET);
	content_buf = malloc(file_sz);
	if (!content_buf)
		return NULL;
	fread(content_buf, 1, file_sz, fp);

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
			continue;  // failed; try next addrinfo

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

		char fpath[MAX_FILE_PATH_LEN + 1];
		char http_req[MAX_HTTP_REQ_LEN];
		memset(http_req, 0, MAX_HTTP_REQ_LEN);
		read(new_fd, http_req, MAX_HTTP_REQ_LEN);
		printf("%s\n", http_req); /* debug */

		parse_req_to_file_path(fpath, http_req);
		printf("file path: %s\n", fpath); /* debug */

		char relative_path[MAX_FILE_PATH_LEN] = "www/cs221.cs.usfca.edu";
		strcat(relative_path, fpath);
		if (!strcmp(fpath, "/"))
			strcat(relative_path, "index.html");

		printf("relative path: %s\n", relative_path); /* debug */

		char content_type[33];
		memset(content_type, 0, 33);

		switch (get_content_type(relative_path)) {
			case 0:
				strncpy(content_type, "text/html", 32);
				break;
			case 1:
				strncpy(content_type, "text/css", 32);
				break;
			case 2:
				strncpy(content_type, "image/png", 32);
				break;
			case 3:
				strncpy(content_type, "text/html", 32);
				break;
			case 4:
				strncpy(content_type, "image/vnd.microsoft.icon", 32);
				break;
		}

		printf("content type: %s\n\n\n\n", content_type); /* debug */

		FILE *fp = fopen(relative_path, "r");
		if (!fp) {
			send_response(new_fd, "404 Not Found", "text/html", "<!DOCTYPE html>\n<html>\n  <body>\n    404 Not found\n  </body>\n</html>\n");
			continue;
		}
		char *cp = get_content(fp, relative_path);
		fclose(fp);
		send_response(new_fd, "200 OK", content_type, cp);
		free(cp);

		close(new_fd);
	}

	close(fd);
	freeaddrinfo(results);
}
