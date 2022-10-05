//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_BEAST_HTTP_BASIC_FILE_BODY_FWD_HPP
#define BOOST_BEAST_HTTP_BASIC_FILE_BODY_FWD_HPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/async_result.hpp>
// fwd declaration so the algos can pick up the TransmitFile/sendfile impls

#if defined(BOOST_BEAST_USE_WIN32_FILE) \
 || defined(BOOST_BEAST_HAS_SENDFILE)

namespace boost {
namespace asio {

template<typename, typename>
struct basic_stream_socket;

}

namespace beast {

#if defined(BOOST_BEAST_HAS_SENDFILE)
class file_posix;
#elif defined(BOOST_BEAST_USE_WIN32_FILE)
class file_win32;
#endif

namespace http {

template<class File>
struct basic_file_body;


template<
        bool isRequest,
        class Body,
        class Fields>
class serializer;

#if defined(BOOST_BEAST_HAS_SENDFILE)

template<
        class Protocol, class Executor,
        bool isRequest, class Fields,
        BOOST_BEAST_ASYNC_TPARAM2 WriteHandler>
BOOST_BEAST_ASYNC_RESULT2(WriteHandler)
async_write_some(
        net::basic_stream_socket<
            Protocol, Executor>& sock,
        serializer<isRequest,
            basic_file_body<file_posix>, Fields>& sr,
        WriteHandler&& handler);

template<
        class Protocol, class Executor,
        bool isRequest, class Fields>
std::size_t
write_some(
        net::basic_stream_socket<Protocol, Executor>& sock,
        serializer<isRequest,
                basic_file_body<file_posix>, Fields>& sr,
        error_code& ec);

#else

template<
        class Protocol, class Executor,
        bool isRequest, class Fields,
        BOOST_BEAST_ASYNC_TPARAM2 WriteHandler>
BOOST_BEAST_ASYNC_RESULT2(WriteHandler)
async_write_some(
        net::basic_stream_socket<
            Protocol, Executor>& sock,
        serializer<isRequest,
            basic_file_body<file_win32>, Fields>& sr,
        WriteHandler&& handler);

template<
        class Protocol, class Executor,
        bool isRequest, class Fields>
std::size_t
write_some(
        net::basic_stream_socket<Protocol, Executor>& sock,
        serializer<isRequest,
                basic_file_body<file_win32>, Fields>& sr,
        error_code& ec);


#endif

}

}

}

#endif

#endif //BOOST_BEAST_HTTP_BASIC_FILE_BODY_FWD_HPP
