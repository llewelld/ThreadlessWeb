# ThreadlessWeb
A simple Webserver library that supports polling as an alternative to threads

## Running a ThreadlessWeb webserver

To use it, compile `threadlessweb.c` in with your code and include the header file.

```
#include "threadlessweb.h"
```

Then use something like the following to run the server.

```
  bool quit = false;
  Webserve * webserve = start_server(80);

  while (quit != true) {
    quit |= poll_once(webserve);
  }

  finish_server(webserve);
```

If your application needs to do some other work at the same time, it should do
short bursts inside the `while` loop, in the usual polling fashion.

## Controlling the server response

If you'd like your sever to respond with something other than the default, you can
do this by creating a conversation callback function. This will get called whenever
a request is received, allowing you to specify the response to send back.

Here's an example of such a callback.

```
bool conversation_callback (WebserveConv * conversation) {
  char * response = "<html><body>Hello</body></html>";
  int length = strlen(response);

  conversation->response = malloc(length + 1);
  strncpy(conversation->response, response, length + 1);
  conversation->response_size = length;

  return true;
}
```

This can be set as the callback to use with the following line of code (which should be
called after `webserve` has been initialised.

```
void set_conv_callback(webserve, conversation_callback);
```

The callback will be called inside the same thread as the webserver (which will be the
same thread that you're calling the poll function from).

To see a complete example, take a look inside the `twexample.c` file.

## The accept-read-write sequence

One other thing to note is that a complete response-request process takes a minimum of 
three polls (accept, read, write). To ensure you catch the full sequence each time, the
convenience function `poll_thrice(webserve)` can be used.

