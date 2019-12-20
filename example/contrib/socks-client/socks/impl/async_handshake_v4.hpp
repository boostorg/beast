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
#include <boost/asio/coroutine.hpp>

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
class socks4_op : public base_type, public net::coroutine
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

    auto request_buffer() -> Buffer&
    {
      return *request_;
    }

    auto response_buffer() -> Buffer&
    {
      return *response_;
    }

    error_code
    prepare_request()
    {
      auto& request = request_buffer();

      auto bytes_to_write = 9 + username_.size();
      auto req = static_cast<char*>(request.prepare(bytes_to_write).data());

      detail::write<uint8_t>(detail::SOCKS_VERSION_4, req); // SOCKS VERSION 4.
      detail::write<uint8_t>(detail::SOCKS_CMD_CONNECT, req); // CONNECT.

      detail::write<uint16_t>(port_, req); // DST PORT.

      auto ec = error_code();

      auto address = net::ip::make_address_v4(hostname_, ec);

      if (not ec)
      {
        detail::write<uint32_t>(address.to_uint(), req); // DST I
        std::copy(username_.begin(), username_.end(), req);    // USERID
        req += username_.size();
        detail::write<uint8_t>(0, req); // NULL.
        request.commit(bytes_to_write);
      }

      return ec;
    }

    auto decode_response() -> error_code
    {
      auto& response = *response_;
      auto resp = static_cast<const unsigned char*>(response.data().data());

      auto result = error_code();

      auto response_version = resp[0];
      if (response_version != 0)
      {
        // response is from a version of socks higher than we recognise.
        // we will mark as an error code but continue decoding. The user can decide
        // whether he wants to check for this error code
        result = error::response_unrecognised_version;
      }

      // response code CD is byte 1 of the response
      auto response_code = resp[1];
      switch(response_code)
      {
      case detail::SOCKS4_REQUEST_GRANTED:
        // success
        break;

      case detail::SOCKS4_REQUEST_REJECTED_OR_FAILED:
          result = error::socks_request_rejected_or_failed;
          break;

      case detail::SOCKS4_CANNOT_CONNECT_TARGET_SERVER:
          result = error::socks_request_rejected_cannot_connect;
          break;

      case detail::SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW:
          result = error::socks_request_rejected_incorrect_userid;
          break;

      default:
          // malformed response
          result = error::socks_unknown_error;
          break;
      }

      return result;
    }

#include <boost/asio/yield.hpp>

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred)
    {
      reenter(this)
      {
        ec = prepare_request();
        if (ec) return this->complete(false, ec);

        yield net::async_write(stream_, request_buffer(), std::move(*this));
        if (ec) return this->complete_now(ec);

        yield net::async_read(stream_, response_buffer(), net::transfer_exactly(8), std::move(*this));
        if(ec) return this->complete_now(ec);

        return this->complete_now(decode_response());
      }
    }

#include <boost/asio/unyield.hpp>

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
}


} // socks


#endif
