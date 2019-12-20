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

template
<
  class Stream,
  class Handler,
  class base_type = boost::beast::stable_async_base
                    <
                      Handler, 
                      typename Stream::executor_type
                    >
>
struct socks4_op 
: base_type
, net::coroutine
{
  // this is necessary because allocator_type is a dependency type of the base class template
  // https://en.cppreference.com/w/cpp/language/dependent_name
  using allocator_type = typename base_type::allocator_type;

  // Rebind the composed operation's allocator type to allow the allocation of chars
  using char_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<char>;

  // build a new string type that shares the composed operation's allocator
  using string_buffer_type = std::basic_string<char, std::char_traits<char>, char_allocator_type>;

  socks4_op(socks4_op&&) = default;
  socks4_op(socks4_op const&) = default;

  socks4_op(Stream& stream, Handler&& handler,
      string_view hostname, unsigned short port,
      string_view username)
      : base_type(std::move(handler), stream.get_executor())
      , stream_(stream)
      , buffer_(boost::beast::allocate_stable<string_buffer_type>(*this, this->get_allocator()))
  {
    auto error = prepare_request(hostname, port, username);
    if (error)
      this->complete(false, error);
    else
      (*this)(error, 0); // start the operation
  }

  // Attempt to build the connect frame in the buffer space.
  // Return an error_code to indicate success
  auto prepare_request(string_view hostname, std::uint16_t port, string_view username) -> error_code
  {
    auto result = error_code();

    auto bytes_to_write = 9 + username.size();
    buffer_.resize(bytes_to_write);
    auto write_ptr = std::addressof(buffer_[0]);
    
    detail::write<uint8_t>(detail::SOCKS_VERSION_4, write_ptr); // SOCKS VERSION 4.
    detail::write<uint8_t>(detail::SOCKS_CMD_CONNECT, write_ptr); // CONNECT.

    detail::write<uint16_t>(port, write_ptr); // DST PORT.

    auto address = net::ip::make_address_v4(
#if BOOST_ASIO_HAS_STRING_VIEW
      hostname, 
#else
      std::string(hostname.begin(), hostname.end()),
#endif        
      result);

    if (not result)
    {
      detail::write<uint32_t>(address.to_uint(), write_ptr); // DST I
      std::copy(username.begin(), username.end(), write_ptr);    // USERID
      write_ptr += username.size();
      detail::write<uint8_t>(0, write_ptr); // NULL.
    }

    return result;
  }

  auto prepare_for_response() -> void
  {
    buffer_.resize(8);
  }

  auto decode_response() -> error_code
  {
    auto result = error_code();

    auto response_version = buffer_[0];
    if (response_version != 0)
    {
      // response is from a version of socks higher than we recognise.
      // we will mark as an error code but continue decoding. The user can decide
      // whether he wants to check for this error code
      result = error::response_unrecognised_version;
    }

    // response code CD is byte 1 of the response
    auto response_code = buffer_[1];
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
        yield net::async_write(stream_, net::buffer(buffer_), std::move(*this));
        if (ec) return this->complete_now(ec);

        prepare_for_response();
        yield net::async_read(stream_, net::buffer(buffer_), std::move(*this));
        if(ec) return this->complete_now(ec);

        return this->complete_now(decode_response());
      }
    }

#include <boost/asio/unyield.hpp>

private:
    // a reference to the stream object passed by the initiator
    Stream&     stream_;

    // a reference to the buffer space object that will be managed by this composed
    // operation
    string_buffer_type& buffer_;
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

    socks4_op<AsyncStream, DecayedHandlerType>
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
