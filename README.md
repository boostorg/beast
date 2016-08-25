# Beast

[![Join the chat at https://gitter.im/vinniefalco/Beast](https://badges.gitter.im/vinniefalco/Beast.svg)](https://gitter.im/vinniefalco/Beast?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge) [![Build Status]
(https://travis-ci.org/vinniefalco/Beast.svg?branch=master)](https://travis-ci.org/vinniefalco/Beast) [![codecov]
(https://codecov.io/gh/vinniefalco/Beast/branch/master/graph/badge.svg)](https://codecov.io/gh/vinniefalco/Beast) [![coveralls]
(https://coveralls.io/repos/github/vinniefalco/Beast/badge.svg?branch=master)](https://coveralls.io/github/vinniefalco/Beast?branch=master) [![Documentation]
(https://img.shields.io/badge/documentation-master-brightgreen.svg)](http://vinniefalco.github.io/beast/) [![License]
(https://img.shields.io/badge/license-boost-brightgreen.svg)](LICENSE_1_0.txt)

A HTTP and WebSocket library for c++ built on boost.asio

## Introduction
Beast is a cross-platform C++ library built on Boost.Asio and Boost, containing two modules implementing widely used network protocols. Beast.HTTP offers a universal model for describing, sending, and receiving HTTP messages while Beast. WebSocket provides a complete implementation of the WebSocket protocol.

*These are the project goals:*

- **Symmetry**: Interfaces are role-agnostic; suitable for use building clients, servers, or *both*.
- **Ease of Use**: Messages are modeled to be simple, accessible objects. Functions and classes used to interact with HTTP or WebSocket messages are designed to resemble Boost.Asio as closely as possible. Users familiar with Boost.Asio will be immediately comfortable using this library.
- **Flexibility**: Beast doesn't require specific implementation strategies; important decisions such as buffer or thread management are left to users of the library.
- **Performance**: Beast is *fast*, making it a good choice for building high performance network servers.
- **Scalability**: Beast is built to efficiently support thousands of concurrent connections, and is a solid base for further abstraction.


## Table of contents
- [Announcements](#announcements)
- [Description](#description)
- [FAQ](#faq)
- [Dependencies](#dependencies)
- [Installation](#install)
- [Usage](#usage)
- [Licence](#licence)
- [Contact](#contact)

## Announcements
#### CppCon 2016

I will be giving a lightning talk on Beast at CppCon 2016 in Bellevue,
Washington from September 18 to September 22. If you'd like to meet me
and hear the talk or ask questions about Beast feel free to approach
me in person or send me an email at vinnie.falco@gmail.com to schedule
some time.

Beast is used in [rippled](https://github.com/ripple/rippled), an
open source server application that implements a decentralized
cryptocurrency system.

About CppCon 2016:
http://cppcon.org

## Description

This software is currently in beta: interfaces are subject to change. For
recent changes see [CHANGELOG](CHANGELOG).
The library has been submitted to the
[Boost Library Incubator](http://rrsd.com/blincubator.com/bi_library/beast-2/?gform_post_id=1579)

* [Project Site](http://vinniefalco.github.io/)
* [Repository](https://github.com/vinniefalco/Beast)
* [Project Documentation](http://vinniefalco.github.io/beast/)
* [Autobahn.testsuite results](http://vinniefalco.github.io/autobahn/index.html)


## FAQ

*please submit issue tickets above*

## Dependencies

Beast provides implementations of the HTTP and WebSocket protocols
built on top of Boost.Asio and other parts of boost.

Usage Requirements:

* Boost
* C++11 or greater
* OpenSSL (optional)

## Installation

Standard project installation example:
1. Copy the Beast library headers to your project include directory, eg:


    / My Project
        / include
            / beast
                / core
                / http
                / websocket
                **.hpp files*
        / src
           **program**
        CMakeLists.txt

2. Edit your CMakeLists file to include Beast header files


    ...
    include_directories(
        ${YOUR_OTHER_INCLUDES}
        include/beast
    )
    ...\

3. That's it! See the usage examples below.

#### Building and running the tests

Build and test scripts can be found in the project scripts directory.

## Usage

Example WebSocket program:
```C++
#include <beast/core/to_string.hpp>
#include <beast/websocket.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

int main()
{
    // Normal boost::asio setup
    std::string const host = "echo.websocket.org";
    boost::asio::io_service ios;
    boost::asio::ip::tcp::resolver r(ios);
    boost::asio::ip::tcp::socket sock(ios);
    boost::asio::connect(sock,
        r.resolve(boost::asio::ip::tcp::resolver::query{host, "80"}));

    // WebSocket connect and send message using beast
    beast::websocket::stream<boost::asio::ip::tcp::socket&> ws(sock);
    ws.handshake(host, "/");
    ws.write(boost::asio::buffer("Hello, world!"));

    // Receive WebSocket message, print and close using beast
    beast::streambuf sb;
    beast::websocket::opcode op;
    ws.read(op, sb);
    ws.close(beast::websocket::close_code::normal);
    std::cout << to_string(sb.data()) << "\n";
}
```

Example HTTP program:
```C++
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

int main()
{
    // Normal boost::asio setup
    std::string const host = "boost.org";
    boost::asio::io_service ios;
    boost::asio::ip::tcp::resolver r(ios);
    boost::asio::ip::tcp::socket sock(ios);
    boost::asio::connect(sock,
        r.resolve(boost::asio::ip::tcp::resolver::query{host, "http"}));

    // Send HTTP request using beast
    beast::http::request_v1<beast::http::empty_body> req;
    req.method = "GET";
    req.url = "/";
    req.version = 11;
    req.headers.replace("Host", host + ":" + std::to_string(sock.remote_endpoint().port()));
    req.headers.replace("User-Agent", "Beast");
    beast::http::prepare(req);
    beast::http::write(sock, req);

    // Receive and print HTTP response using beast
    beast::streambuf sb;
    beast::http::response_v1<beast::http::streambuf_body> resp;
    beast::http::read(sock, sb, resp);
    std::cout << resp;
}
```

## Licence

The Beast licence can be found [here]('https://github.com/vinniefalco/Beast/blob/master/LICENSE_1_0.txt').
## Contact

Please report issues or questions here:
https://github.com/vinniefalco/Beast/issues
