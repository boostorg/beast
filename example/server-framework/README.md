<img width="880" height = "80" alt = "Beast"
    src="https://raw.githubusercontent.com/vinniefalco/Beast/master/doc/images/readme.png">

# HTTP and WebSocket built on Boost.Asio in C++11

## Server-Framework

This example is a complete, multi-threaded server built with Beast.
It contains the following components

* WebSocket ports (synchronous and asynchronous)
    - Echoes back any message received

* HTTP ports (synchronous and asynchronous)
    - Serves files from a configurable directory on GET request
    - Responds to HEAD requests with the appropriate result
    - Routes WebSocket Upgrade requests to a WebSocket port
    - Handles Expect: 100-continue
    - Supports pipelined requests

The server is designed to use modular components that users may simply copy
into their own project to get started quickly. Two concepts are introduced:

## PortHandler

The **PortHandler** concept defines an algorithm for handling incoming
connections received on a listening socket. The example comes with four
port handlers:

* `http_sync_port` Serves HTTP content using synchronous Beast calls

* `http_async_port` Serves HTTP content using asynchronous Beast calls

* `ws_sync_port` A synchronous WebSocket echo server using synchronous Beast calls

* `ws_async_port` An asynchronous WebSocket echo server using synchronous Beast calls

A port handler takes the stream object resulting form an incoming connection
request and constructs a handler-specific connection object which provides
the desired behavior.

## Service

The HTTP ports which come with the example have a system built in which allows
installation of framework and user-defined "HTTP services". These services
inform connections using the port on how to handle specific requests. This is
similar in concept to an "HTTP router" which is an element of most modern
servers.

These HTTP services are represented by the **Service** concept, and managed
in a container holding a type-list, called a `service_list`. Each HTTP port
allows the sevice list to be defined at compile-time and initialized at run
time. The framework provides these services:

* `file_service` Produces HTTP responses delivering files from a system path

* `ws_upgrade_service` Transports a connection requesting a WebSocket Upgrade
to a websocket port handler.

## Relationship

This diagram shows the relationship of the server object, to the four
ports created in the example program, and the HTTP services contained by
the HTTP ports:

<img width="880" height = "344" alt = "ServerFramework"
    src="https://raw.githubusercontent.com/vinniefalco/Beast/server/doc/images/server.png">

