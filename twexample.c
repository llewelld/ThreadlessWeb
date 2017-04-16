/**
 * @file
 * @author  David Llewellyn-Jones <david@flypig.co.uk>
 * @version 1.0
 *
 * @section LICENSE
 *
 * @brief Example use of ThreadlessWeb
 * @section DESCRIPTION
 *
 * Provides a simple example of ThreadlessWeb, creating a simple but functional
 * Webserver. It always returns the same droll response to any GET or POST
 * request.
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

// Structure definitions

static bool quit;

// Function prototypes

static void interrupt (int sig, siginfo_t * siginfo, void * context);
static void configure_interrupt();
static void display_help();

// Function definitions

int main(int argc, char **argv) {
	Webserve * webserve;
	int port;

	quit = false;

	// Get the port to use from the command line
	port = 0;
	if (argc == 2) {
		port = atoi(argv[1]);
	}

	if (port == 0) {
		display_help();
	}
	else {
		configure_interrupt();

		// Start up
		printf("INFO: Webserver starting on port %d, pid %d\n", port, getpid());
		webserve = start_server(port);
		// Set polls to block for 1 second
		set_timeout_usec(webserve, 1E6);

		// Poll for connections
		while (quit != true) {
			quit |= poll_once(webserve);
		}

		// Clear up
		finish_server(webserve);
		printf("INFO: Webserver closed down\n");
	}
}

static void interrupt (int sig, siginfo_t * siginfo, void * context) {
	printf("\nINFO: Interrupt signal received\n");
	quit = true;
}

static void configure_interrupt() {
	struct sigaction act;

	// Set up a signal handler so we can close the listening socket
	memset (&act, '\0', sizeof(act));
	act.sa_sigaction = &interrupt;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGINT, &act, NULL) < 0) {
		printf("ERROR: Sigaction");
		exit(3);
	}
}

static void display_help() {
	printf("Syntax: threadlessweb <port>\n"
		"Runs a simple webserver that always responds in the same way.\n"
		"Example: threadlessweb 1337\n"
		"");
}

