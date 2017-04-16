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

#ifndef __THREADLESSWEB_H
#define __THREADLESSWEB_H (1)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <arpa/inet.h>

// Defines

#define VERSION 23
#define RESPONSE_ERROR      42
#define RESPONSE_FORBIDDEN 403

// Structure definitions

typedef enum {
	REQUEST_INVALID = -1,
	
	REQUEST_GET,
	REQUEST_POST,
	
	REQUEST_NUM
} REQUEST;

typedef struct _Webserve Webserve;

typedef struct _WebserveConv {
	int hit;
	REQUEST type;
	char * request_header;
	size_t request_header_size;
	char * request_body;
	size_t request_body_size;
	int response_code;
	char * response;
	size_t response_size;
} WebserveConv;

typedef bool (*WebservConvCallback)(WebserveConv * conversation);

// Function prototypes

// Run the server
Webserve * start_server(int port);
bool poll_once(Webserve * webserve);
bool poll_thrice(Webserve * webserve);
void poll_forever(Webserve * webserve);
void finish_server(Webserve * webserve);

// Configure the server
void set_timeout_usec(Webserve * webserve, unsigned int usec);
void set_conv_callback(Webserve * webserve, WebservConvCallback conversation_callback);

// Function definitions

#endif

