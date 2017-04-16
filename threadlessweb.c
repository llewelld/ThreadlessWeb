/**
 * @file
 * @author  David Llewellyn-Jones <david@flypig.co.uk>
 * @version 1.0
 *
 * @section LICENSE
 *
 * @brief Simple threadless Webserver
 * @section DESCRIPTION
 *
 * Provides a library for incorporating a simple threadless Webserver into your
 * code. Works on a polling basis using select and a user-definable timeout.
 * A callback can be set that's triggered on every request and allows a response
 * to be crafted based on what's received.
 *
 * It's a really simple webserver. Probably don't use it for production
 * environments.
 *
 * I used a couple of really great examples from the Web to help with this code.
 * For example use of select() see
 * https://www.gnu.org/software/libc/manual/html_node/Server-Example.html
 * For example webserver code see
 * https://gist.github.com/sumpygump/9908417
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "threadlessweb.h"

// Defines

#define BUFSIZE 8096

#define RESPONSE_CONTENT "Okay\n"
#define RESPONSE_TYPE "text/html"
#define RESPONSE_LENGTH sizeof(RESPONSE_CONTENT)

#define FORBIDDEN_TEXT "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n"
#define FORBIDDEN_TEXT_LENGTH sizeof(FORBIDDEN_TEXT)

#if defined(_WIN32) || defined(_WIN64)
#define LOG(level_, ...) printf(__VA_ARGS__);
#else
#include <syslog.h>
#define LOG(level_, ...) syslog((level_), __VA_ARGS__)
#endif

// Structure definitions

static char const * const requests[] = {
	"GET ",
	"POST "
};

struct _Webserve {
	int listenfd;
	int hit;
	fd_set active_read_fd_set;
	fd_set active_write_fd_set;
	unsigned int timeout_usec;
	WebservConvCallback conversation_callback;
	WebserveConv * conversation[FD_SETSIZE];
	bool quit;
};

// Function prototypes

void * receive_request(void * t);
void spawn_receive(int fd, int hit);
void forbidden(int socket_fd);
Webserve * check_connect(int listenfd);
void web_read(int fd, WebserveConv * conversation);
void web_write(int fd, WebserveConv * conversation);
void conversation_new(Webserve * webserve, int fd);
void conversation_clear(Webserve * webserve, int fd);
bool default_conv_callback(WebserveConv * request);

// Function definitions

void web_read(int fd, WebserveConv * conversation) {
	long i, ret;
	// Static buffer is zero filled
	static char buffer[BUFSIZE + 1];
	int request;
	REQUEST type;
	int size;
	int prefixsize;
	int hit;
	int boundary;
	int retcount;
	
	hit = 0;
	if (conversation) {
		hit = conversation->hit;
	}

	// Read full Web request in one go
	ret = read(fd, buffer, BUFSIZE);
	if (ret == 0 || ret == -1) {	/* read failure stop now */
		forbidden(fd);
		LOG(LOG_WARNING, "FORBIDDEN: Failed to read browser request, %d\n", fd);
		exit(3);
	}

	if (ret > 0 && ret < BUFSIZE)	{
		// Return code is valid chars
		// Terminate the buffer
		buffer[ret]=0;
	}
	else {
		buffer[0] = 0;
	}
	// Find the boundary between header and body
	boundary = ret;
	retcount = 0;
	i = 0;
	while ((boundary == ret) && (i < ret)) {
		if ((buffer[i] == '\r') || (buffer[i] == '\n')) {
			retcount++;
		}
		else {
			if (retcount >= 4) {
				boundary = i;
			}
			else {
				retcount = 0;
			}
		}
		i++;
	}

	// Copy to the conversation structure
	if (conversation) {
		conversation->request_header = malloc(boundary + 1);
		if (conversation->request_header) {
			memcpy(conversation->request_header, buffer, boundary);
			conversation->request_header[boundary] = '\0';
			conversation->request_header_size = boundary;
		}

		conversation->request_body = malloc(ret - boundary + 1);
		if (conversation->request_body) {
			memcpy(conversation->request_body, buffer + boundary, ret - boundary);
			conversation->request_body[ret - boundary] = '\0';
			conversation->request_body_size = (ret - boundary);
		}
	}
	
	// Remove CF and LF characters
	for (i = 0; i < ret; i++) {
		if (buffer[i] == '\r' || buffer[i] == '\n') {
			buffer[i]='*';
		}
	}
	LOG(LOG_INFO, "Request %d: %s\n", hit, buffer);

	// Figure out what sort of request this is (GET, POST?)
	type = REQUEST_INVALID;
	prefixsize = 0;
	for (request = 0; (request < REQUEST_NUM) && (type == REQUEST_INVALID); request++) {
		size = strlen(requests[request]);
		if (strncasecmp(buffer, requests[request], size) == 0) {
			prefixsize = size;
			type = request;
		}
	}
	
	if (conversation) {
		conversation->type = type;
	}

	// Not all HTTP operations are supported
	if ((type <= REQUEST_INVALID) || (type >= REQUEST_NUM)) {
		forbidden(fd);
		LOG(LOG_WARNING, "FORBIDDEN: Operation not supported: %s: %d\n", buffer, fd);
		exit(3);
	}
	// null terminate after the second space to ignore extra stuff
	for (i = prefixsize; i < BUFSIZE; i++) {
		if (buffer[i] == ' ') {
			// String is "GET URL " + lots of other stuff
			buffer[i] = 0;
			break;
		}
	}
}

void web_write(int fd, WebserveConv * conversation) {
	// Static buffer is zero filled
	static char buffer[BUFSIZE + 1];
	int written;
	size_t length;
	char * content;
	int hit;
	
	content = RESPONSE_CONTENT;
	length = RESPONSE_LENGTH;
	if (conversation != NULL) {
		hit = conversation->hit;
		if (conversation->response) {
			content = conversation->response;
			length = conversation->response_size;
		}
	}

	// Craft a response (Header + a blank line)
	sprintf(buffer, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, length, RESPONSE_TYPE);
	written = write(fd, buffer, strlen(buffer));
	written = write(fd, content, length);

	if (written < 0) {
		LOG(LOG_ERR, "ERROR: Write\n");
	}

	// Allow socket to drain before signalling the socket is closed
	//sleep(1);
	close(fd);
	LOG(LOG_INFO, "INFO: Request %d closed\n", hit);
}

void forbidden(int socket_fd) {
	int written;

	written = write(socket_fd, FORBIDDEN_TEXT, FORBIDDEN_TEXT_LENGTH);
	LOG(LOG_INFO, "INFO: Forbidden, wrote response size %d\n", written);
}

void set_timeout_usec(Webserve * webserve, unsigned int usec) {
	if (webserve) {
		// Microseconds
		webserve->timeout_usec = usec;
	}
}

Webserve * start_server(int port) {
	int listenfd;
	// static = initialised to zeros
	static struct sockaddr_in serv_addr;
	Webserve * webserve;

	webserve = NULL;

	// Setup the network socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		LOG(LOG_ERR, "ERROR: System call socket\n");
		exit(3);
	}

	if (port < 0 || port > 60000) {
		LOG(LOG_ERR, "ERROR: Invalid port number (try 1->60000): %d\n", port);
		exit(3);
	}

	// Bind to the listening socket
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		LOG(LOG_ERR, "ERROR: System call: bind\n");
		exit(3);
	}

	// Listen for connections
	if (listen(listenfd, 64) <0 ) {
		LOG(LOG_ERR, "ERROR: System call: listen\n");
		exit(3);
	}

	// Go in to the main listening loop
	webserve = check_connect(listenfd);

	return webserve;
}

void finish_server(Webserve * webserve) {
	webserve->quit = true;
	close(webserve->listenfd);
	free(webserve);
}

Webserve * check_connect(int listenfd) {
	Webserve * webserve;

	webserve = calloc(sizeof(Webserve), 1);
	
	webserve->listenfd = listenfd;
	webserve->hit = 0;

	FD_ZERO(&webserve->active_write_fd_set);
	FD_ZERO(&webserve->active_read_fd_set);
	FD_SET(listenfd, &webserve->active_read_fd_set);

	// Microseconds
	webserve->timeout_usec = 1E6;
	
	// Set the default conversation callback
	webserve->conversation_callback = default_conv_callback;
	
	webserve->quit = false;

	return webserve;
}

void poll_forever(Webserve * webserve) {
	while (webserve->quit != true) {
		webserve->quit |= poll_once(webserve);
	}
}

bool poll_thrice(Webserve * webserve) {
	int count;
	for (count = 0; (count < 3) && (webserve->quit != true); count++) {
		webserve->quit |= poll_once(webserve);
	}

	return webserve->quit;
}

bool poll_once(Webserve * webserve) {
	struct timeval timeout;
	int i;
	int fd;
	socklen_t size;
	struct sockaddr_in clientname;
	fd_set read_fd_set;
	fd_set write_fd_set;
	bool conv_result;

	timeout.tv_sec = webserve->timeout_usec / 1E6;
	timeout.tv_usec = webserve->timeout_usec - (timeout.tv_sec * 1E6);

	read_fd_set = webserve->active_read_fd_set;
	write_fd_set = webserve->active_write_fd_set;
	if (select(FD_SETSIZE, &read_fd_set, &write_fd_set, NULL, &timeout) < 0) {
		LOG(LOG_ERR, "ERROR: Select\n");
		webserve->quit = true;
	}

	// Service all the sockets with input pending
	for (i = 0; (i < FD_SETSIZE) && (webserve->quit != true); ++i) {
		if (FD_ISSET (i, &read_fd_set)) {
			if (i == webserve->listenfd) {
				// Connection request on original socket
				size = sizeof (clientname);
				fd = accept (i, (struct sockaddr *) &clientname, &size);
				if (fd < 0) {
					LOG(LOG_ERR, "ERROR: Accept\n");
					exit (EXIT_FAILURE);
				}
				webserve->hit++;
				LOG(LOG_INFO, "INFO: Request %d connection from %s\n", webserve->hit, inet_ntoa(clientname.sin_addr));
				FD_SET (fd, &webserve->active_read_fd_set);

				// Start a conversation
				conversation_new (webserve, fd);
			}
			else {
				web_read(i, webserve->conversation[i]);
				FD_CLR (i, &webserve->active_read_fd_set);

				if (webserve->conversation_callback) {
					conv_result = webserve->conversation_callback(webserve->conversation[i]);
				}

				if ((webserve->conversation_callback == NULL) || (conv_result = false)) {
					conv_result = default_conv_callback (webserve->conversation[i]);
				}

				FD_SET (i, &webserve->active_write_fd_set);
			}
		}

		if (FD_ISSET (i, &write_fd_set)) {
			web_write(i, webserve->conversation[i]);
			FD_CLR (i, &webserve->active_write_fd_set);

			// Finish the conversation
			conversation_clear(webserve, i);
		}
	}
	
	return webserve->quit;
}

void conversation_new (Webserve * webserve, int fd) {
	if ((fd >= 0) && (fd < FD_SETSIZE) && (webserve != NULL)) {
		if (webserve->conversation[fd] != NULL) {
			// There's an old lingering conversation, which we must clear
			if (webserve->conversation[fd]->request_header) {
				free(webserve->conversation[fd]->request_header);
			}
			if (webserve->conversation[fd]->request_body) {
				free(webserve->conversation[fd]->request_body);
			}
			if (webserve->conversation[fd]->response) {
				free(webserve->conversation[fd]->response);
			}
			free(webserve->conversation[fd]);
		}
			
		// Create the new conversation structure
		webserve->conversation[fd] = calloc(sizeof(WebserveConv), 1);
		webserve->conversation[fd]->hit = webserve->hit;
		webserve->conversation[fd]->type = RESPONSE_ERROR;
	}
}

void conversation_clear(Webserve * webserve, int fd) {
	if ((fd > 0) && (fd < FD_SETSIZE) && (webserve != NULL)) {
		if (webserve->conversation[fd] != NULL) {
			// Clear the conversation content
			if (webserve->conversation[fd]->request_header) {
				free(webserve->conversation[fd]->request_header);
			}
			if (webserve->conversation[fd]->request_body) {
				free(webserve->conversation[fd]->request_body);
			}
			if (webserve->conversation[fd]->response) {
				free(webserve->conversation[fd]->response);
			}
			free(webserve->conversation[fd]);
			
			webserve->conversation[fd] = NULL;
		}
	}
}

void set_conv_callback(Webserve * webserve, WebservConvCallback conversation_callback) {
	if (webserve != NULL) {
		if (conversation_callback != NULL) {
			webserve->conversation_callback = conversation_callback;
		}
		else {
			webserve->conversation_callback = default_conv_callback;
		}
	}
}

bool default_conv_callback (WebserveConv * conversation) {
	conversation->response = malloc(RESPONSE_LENGTH + 1);
	strncpy(conversation->response, RESPONSE_CONTENT, RESPONSE_LENGTH);
	conversation->response_size = RESPONSE_LENGTH;
	return true;
}

