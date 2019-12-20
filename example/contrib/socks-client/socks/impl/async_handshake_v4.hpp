//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
// Copyright (c) 2019 Richard Hodges (hodges dot r at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_IMPL_ASYNC_HANDSHAKE_V4_HPP
#define SOCKS_IMPL_ASYNC_HANDSHAKE_V4_HPP

#include <socks/detail/protocol.hpp>
#include <boost/beast/core/async_base.hpp>
#include <boost/beast/core/detail/is_invocable.hpp>
#include <boost/config.hpp> // for BOOST_FALLTHROUGH
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <utility>
#include <type_traits>

#include "../detail/read_write.hpp"

namespace socks {

template<
    class Stream,
    class Handler,
    class Buffer,
    class base_type = boost::beast::async_base<
        Handler, typename Stream::executor_type>>
class socks4_op : public base_type
{
public:
    socks4_op(socks4_op&&) = default;
    socks4_op(socks4_op const&) = default;

    socks4_op(Stream& stream, Handler&& handler,
        string_view hostname, unsigned short port,
        string_view username)
        : base_type(std::move(handler), stream.get_executor())
        , stream_(stream)
        , hostname_(hostname)
        , port_(port)
        , username_(username)
    {
        (*this)({}, 0); // start the operation
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        using detail::write;
        using detail::read;

        auto& response = *response_;
        auto& request = *request_;

        switch (ec ? 3 : step_)
        {
        case 0:
        {
            step_ = 1;

            std::size_t bytes_to_write = 9 + username_.size();
            auto req = static_cast<char*>(request.prepare(bytes_to_write).data());

            write<uint8_t>(detail::SOCKS_VERSION_4, req); // SOCKS VERSION 4.
            write<uint8_t>(detail::SOCKS_CMD_CONNECT, req); // CONNECT.

            write<uint16_t>(port_, req); // DST PORT.

            auto address = net::ip::make_address_v4(hostname_, ec);
            if (ec)
                break;

            write<uint32_t>(address.to_uint(), req); // DST I

            if (!username_.empty())
            {
                std::copy(username_.begin(), username_.end(), req);    // USERID
                req += username_.size();
            }
            write<uint8_t>(0, req); // NULL.

            request.commit(bytes_to_write);
            return net::async_write(stream_, request, std::move(*this));
        }
        case 1:
        {
            step_ = 2;
            return net::async_read(stream_, response,
                net::transfer_exactly(8), std::move(*this));
        }
        case 2:
        {
            auto resp = static_cast<const unsigned char*>(response.data().data());

            read<uint8_t>(resp); // VN is the version of the reply code and should be 0.
            auto cd = read<uint8_t>(resp);

            if (cd != detail::SOCKS4_REQUEST_GRANTED)
            {
                switch (cd)
                {
                case detail::SOCKS4_REQUEST_REJECTED_OR_FAILED:
                    ec = error::socks_request_rejected_or_failed;
                    break;
                case detail::SOCKS4_CANNOT_CONNECT_TARGET_SERVER:
                    ec = error::socks_request_rejected_cannot_connect;
                    break;
                case detail::SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW:
                    ec = error::socks_request_rejected_incorrect_userid;
                    break;
                default:
                    ec = error::socks_unknown_error;
                    break;
                }
            }
            BOOST_FALLTHROUGH;
        }
        case 3:
            break;
        }

        this->complete_now(ec);
    }

private:
    Stream& stream_;

    using BufferPtr = std::unique_ptr<Buffer>;
    BufferPtr request_{ new Buffer() }; // std::make_unique c++14 or later.
    BufferPtr response_{ new Buffer() };

    std::string hostname_;
    unsigned short port_;
    std::string username_;
    int step_ = 0;
};

/** Perform the SOCKS v4 handshake in the client role.
*/

struct async_handshake_v4_initiator
{
  template<class HandlerType, class AsyncStream>
  void operator()(
    HandlerType&& handler,
    AsyncStream& stream,
    string_view hostname,
    unsigned short port,
    string_view username
  )
  {
      using DecayedHandlerType = typename std::decay<HandlerType>::type;

      using Buffer = net::basic_streambuf<typename std::allocator_traits<
        net::associated_allocator_t<DecayedHandlerType>>:: template rebind_alloc<char> >;

      socks4_op<AsyncStream, DecayedHandlerType, Buffer>
      (
        stream, 
        std::forward<HandlerType>(handler),
        hostname, 
        port, 
        username
      );
  }

};

template<
    typename AsyncStream,
    typename Handler
>
BOOST_BEAST_ASYNC_RESULT1(Handler)
async_handshake_v4(
    AsyncStream& stream,
    string_view hostname,
    unsigned short port,
    string_view username,
    Handler&& handler)
{
  return net::async_initiate<Handler, void(error_code)>
  (
    async_handshake_v4_initiator(),
    handler,
    stream,
    hostname,
    port,
    username
  );
  /*
    net::async_completion<Handler, void(error_code)> init{ handler };

    using HandlerType = typename std::decay<decltype(init.completion_handler)>::type;


    using Buffer = net::basic_streambuf<typename std::allocator_traits<
        net::associated_allocator_t<HandlerType>>:: template rebind_alloc<char> >;

    socks4_op<AsyncStream, HandlerType, Buffer>(stream, init.completion_handler,
        hostname, port, username);

    return init.result.get();
  */
}


} // socks

#endif
