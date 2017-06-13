<img width="880" height = "80" alt = "Beast"
    src="https://raw.githubusercontent.com/vinniefalco/Beast/master/doc/images/readme.png">

[![Join the chat at https://gitter.im/vinniefalco/Beast](https://badges.gitter.im/vinniefalco/Beast.svg)](https://gitter.im/vinniefalco/Beast?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Build Status](https://travis-ci.org/vinniefalco/Beast.svg?branch=master)](https://travis-ci.org/vinniefalco/Beast)
[![Build status](https://ci.appveyor.com/api/projects/status/g0llpbvhpjuxjnlw?svg=true)](https://ci.appveyor.com/project/vinniefalco/beast)
[![codecov](https://codecov.io/gh/vinniefalco/Beast/branch/master/graph/badge.svg)](https://codecov.io/gh/vinniefalco/Beast)
[![Documentation](https://img.shields.io/badge/documentation-master-brightgreen.svg)](http://vinniefalco.github.io/beast/)
[![License](https://img.shields.io/badge/license-boost-brightgreen.svg)](LICENSE_1_0.txt)

# HTTP and WebSocket built on Boost.Asio in C++11

## Contents

- [Introduction](#introduction)
- [Audience](audience)
- [Appearances](#appearances)
- [Description](#description)
- [Requirements](#requirements)
- [Building](#building)
- [Usage](#usage)
- [Licence](#licence)
- [Contact](#contact)
- [Contributing](#Contributing)

## Introduction

Beast is a cross-platform, header-only C++11 library for low-level
HTTP/1 and WebSocket protocol programming
using the consistent asynchronous networking model of Boost.Asio.
Beast is not an HTTP client or HTTP server, but it can be used to
build those things. It is intended to be a foundation for writing
other interoperable libraries by providing HTTP vocabulary types
and algorithms. The provided examples show how clients and servers
might be built.

This library is designed for:

* **Symmetry:** Interfaces are role-agnostic; build clients, servers, or both.

* **Ease of Use:** Boost.Asio users will immediately understand Beast.

* **Flexibility:** Users make the important decisions such as buffer or
  thread management.

* **Performance:** Build applications handling thousands of connections or more.

* **Basis for Further Abstraction.** Components are open-ended and
  suited for building higher level libraries.

## Audience

Beast is for network programmers who have some familiarity with
Boost.Asio. In particular, users who wish to write asynchronous programs
with Beast should already know how to use Asio sockets and streams,
and should know how to create concurrent network programs using
Asio callbacks or coroutines.

## Appearances

| <a href="http://cppcast.com/2017/01/vinnie-falco/">CppCast 2017</a> | <a href="https://raw.githubusercontent.com/vinniefalco/Beast/master/doc/images/CppCon2016.pdf">CppCon 2016</a> |
| ------------ | ----------- |
| <a href="http://cppcast.com/2017/01/vinnie-falco/"><img width="180" height="180" alt="Vinnie Falco" src="https://avatars1.githubusercontent.com/u/1503976?v=3&u=76c56d989ef4c09625256662eca2775df78a16ad&s=180"></a> | <a href="https://www.youtube.com/watch?v=uJZgRcvPFwI"><img width="320" height = "180" alt="Beast" src="https://raw.githubusercontent.com/vinniefalco/Beast/master/doc/images/CppCon2016.png"></a> |

## Description

This software is currently in beta: interfaces may change.
For recent changes see the [CHANGELOG](CHANGELOG.md).
The library has been submitted to the
[Boost Library Incubator](http://rrsd.com/blincubator.com/bi_library/beast-2/?gform_post_id=1579)

* [Project Site](http://vinniefalco.github.io/)
* [Repository](https://github.com/vinniefalco/Beast)
* [Project Documentation](http://vinniefalco.github.io/beast/)
* [Autobahn.testsuite results](http://vinniefalco.github.io/autobahn/index.html)

## Requirements

* **C++11:** Robust support for most language features.
* **Boost:** Boost.Asio and some other parts of Boost.
* **OpenSSL:** Optional, for using TLS/Secure sockets.

When using Microsoft Visual C++, Visual Studio 2015 Update 3 or later is required.

These components are required in order to build the tests and examples:

* CMake 3.7.2 or later
* Properly configured bjam/b2

## Building

Beast is header-only so there are no libraries to build or link with.
To use Beast in your project, simply copy the Beast sources to your
project's source tree (alternatively, bring Beast into your Git repository
using the `git subtree` or `git submodule` commands). Then, edit your
 build scripts to add the `include/` directory to the list of paths checked
 by the C++ compiler when searching for includes. Beast `#include` lines
 will look like this:
```C++
#include <beast/http.hpp>
#include <beast/websocket.hpp>
```

To link your program successfully, you'll need to add the Boost.System
library to link with. If you use coroutines you'll also need the
Boost.Coroutine library. Please visit the Boost documentation for
instructions on how to do this for your particular build system.

For the examples and tests, Beast provides build scripts for Boost.Build (bjam)
and CMake. It is possible to generate Microsoft Visual Studio or Apple
Xcode project files using CMake by executing these commands from
the root of the repository:

```
mkdir bin
cd bin
cmake ..                                    # for 32-bit Windows builds
cmake -G Xcode ..                           # for Apple Xcode builds

cd ..
mkdir bin64
cd ../bin64
cmake -G"Visual Studio 14 2015 Win64" ..    # for 64-bit Windows builds (VS2015)
cmake -G"Visual Studio 15 2017 Win64" ..    # for 64-bit Windows builds (VS2017)

```

To build with Boost.Build, it is necessary to have the bjam executable
in your path. And bjam needs to know how to find the Boost sources. The
easiest way to do this is make sure that the version of bjam in your path
is the one at the root of the Boost source tree, which is built when
running `bootstrap.sh` (or `bootstrap.bat` on Windows).

Once bjam is in your path, simply run bjam in the root of the Beast
repository to automatically build the required Boost libraries if they
are not already built, build the examples, then build and run the unit
tests.

The files in the repository are laid out thusly:

```
./
    bin/            Create this to hold executables and project files
    bin64/          Create this to hold 64-bit Windows executables and project files
    doc/            Source code and scripts for the documentation
    include/        Add this to your compiler includes
        beast/
    extras/         Additional APIs, may change
    examples/       Self contained example programs
    test/           Unit tests and benchmarks
```


## Usage

These examples are complete, self-contained programs that you can build
and run yourself (they are in the `examples` directory).

Example WebSocket program:
```C++
#include <beast/core.hpp>
#include <beast/websocket.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

int main()
{
    // Normal boost::asio setup
    std::string const host = "echo.websocket.org";
    boost::asio::io_service ios;
    boost::asio::ip::tcp::resolver r{ios};
    boost::asio::ip::tcp::socket sock{ios};
    boost::asio::connect(sock,
        r.resolve(boost::asio::ip::tcp::resolver::query{host, "80"}));

    // WebSocket connect and send message using beast
    beast::websocket::stream<boost::asio::ip::tcp::socket&> ws{sock};
    ws.handshake(host, "/");
    ws.write(boost::asio::buffer(std::string("Hello, world!")));

    // Receive WebSocket message, print and close using beast
    beast::multi_buffer b;
    beast::websocket::opcode op;
    ws.read(op, b);
    ws.close(beast::websocket::close_code::normal);
    std::cout << beast::buffers(b.data()) << "\n";
}
```

Example HTTP program:
```C++
#include <beast/core.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <string>

int main()
{
    // Normal boost::asio setup
    std::string const host = "www.example.com";
    boost::asio::io_service ios;
    boost::asio::ip::tcp::resolver r{ios};
    boost::asio::ip::tcp::socket sock{ios};
    boost::asio::connect(sock,
        r.resolve(boost::asio::ip::tcp::resolver::query{host, "http"}));

    // Send HTTP request using beast
    beast::http::request<beast::http::string_body> req;
    req.method(beast::http::verb::get);
    req.target("/");
    req.version = 11;
    req.insert(beast::http::field::host, host + ":" +
        boost::lexical_cast<std::string>(sock.remote_endpoint().port()));
    req.insert(beast::http::field::user_agent, "Beast");
    req.prepare();
    beast::http::write(sock, req);

    // Receive and print HTTP response using beast
    beast::flat_buffer b;
    beast::http::response<beast::http::dynamic_body> res;
    beast::http::read(sock, b, res);
    std::cout << res << std::endl;
}
```

## License

Distributed under the Boost Software License, Version 1.0.
(See accompanying file [LICENSE_1_0.txt](LICENSE_1_0.txt) or copy at
http://www.boost.org/LICENSE_1_0.txt)

## Contact

Please report issues or questions here:
https://github.com/vinniefalco/Beast/issues


---

## Contributing (We Need Your Help!)

If you would like to contribute to Beast and help us maintain high
quality, consider performing code reviews on active pull requests.
Any feedback from users and stakeholders, even simple questions about
how things work or why they were done a certain way, carries value
and can be used to improve the library. Code review provides these
benefits:

* Identify bugs
* Documentation proof-reading
* Adjust interfaces to suit use-cases
* Simplify code

You can look through the Closed pull requests to get an idea of how
reviews are performed. To give a code review just sign in with your
GitHub account and then add comments to any open pull requests below,
don't be shy!
<p>https://github.com/vinniefalco/Beast/pulls</p>

Here are some resources to learn more about
code reviews:

* <a href="https://blog.scottnonnenberg.com/top-ten-pull-request-review-mistakes/">Top 10 Pull Request Review Mistakes</a>
* <a href="https://smartbear.com/SmartBear/media/pdfs/best-kept-secrets-of-peer-code-review.pdf">Best Kept Secrets of Peer Code Review (pdf)</a>
* <a href="http://support.smartbear.com/support/media/resources/cc/11_Best_Practices_for_Peer_Code_Review.pdf">11 Best Practices for Peer Code Review (pdf)</a>
* <a href="http://www.evoketechnologies.com/blog/code-review-checklist-perform-effective-code-reviews/">Code Review Checklist – To Perform Effective Code Reviews</a>
* <a href="https://www.codeproject.com/Articles/524235/Codeplusreviewplusguidelines">Code review guidelines</a>
* <a href="https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md">C++ Core Guidelines</a>
* <a href="https://doc.lagout.org/programmation/C/CPP101.pdf">C++ Coding Standards (Sutter & Andrescu)</a>

Beast thrives on code reviews and any sort of feedback from users and
stakeholders about its interfaces. Even if you just have questions,
asking them in the code review or in issues provides valuable information
that can be used to improve the library - do not hesitate, no question
is insignificant or unimportant!

While code reviews are the preferred form of donation, if you simply
must donate money to support the library, please do so
using <a href="https://bitcoin.org">Bitcoin</a> sent to this address:
<a href="bitcoin:1DaPsDvv6MjFUSnsxXSHzeYKSjzrWrQY7T?amount=0.03&label=Beast%20Library"><b>1DaPsDvv6MjFUSnsxXSHzeYKSjzrWrQY7T</b></a>

<a href="bitcoin:1DaPsDvv6MjFUSnsxXSHzeYKSjzrWrQY7T?amount=0.03&label=Beast%20Library">
    <img src="https://raw.githubusercontent.com/vinniefalco/Beast/master/doc/images/btc_qr2.png" width="490" height="100"></a>
