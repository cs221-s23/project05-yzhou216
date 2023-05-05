#define MAX_URI_LEN 128
#define MAX_FILE_PATH_LEN 256
#define MAX_CONTENT_TYPE_LEN 32
#define MAX_STATUS_LEN 32

struct request {
	char uri[MAX_CONTENT_TYPE_LEN + 1];
	char path[MAX_FILE_PATH_LEN + 1];
	char content_type[MAX_CONTENT_TYPE_LEN + 1];
} request;

struct response {
	char status[MAX_STATUS_LEN + 1];
	char *content;
} response;
