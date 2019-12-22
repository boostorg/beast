//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
// Copyright (c) 2019 Richard Hodges (hodges dot r at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// An implementation of the SOCKS4 handshake protocol
// @see https://tools.ietf.org/html/rfc1413

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

#include <ciso646>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <type_traits>

namespace socks {

template
<
  class Stream,
  class Handler,
  class base_type = boost::beast::async_base
                    <
                      Handler,
                      typename Stream::executor_type
                    >
>
struct socks4_op
: base_type
, boost::asio::coroutine
{
  // this is necessary because allocator_type is a dependency type of the base class template
  // https://en.cppreference.com/w/cpp/language/dependent_name
  using allocator_type = typename base_type::allocator_type;
  using executor_type = typename base_type::executor_type;

  // Rebind the composed operation's allocator type to allow the allocation of chars
  using char_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<char>;

  // build a new string type that shares the composed operation's allocator
  using string_buffer_type = std::basic_string<char, std::char_traits<char>, char_allocator_type>;

  // we want the resolver's async handler to be invoked on our executor
  using resolver_type = net::ip::tcp::resolver;

  socks4_op(socks4_op&&) = default;

  socks4_op(socks4_op const&) = default;

  socks4_op(Stream &stream, Handler &&handler,
            string_view hostname, string_view service,
            string_view username)
    : base_type(std::move(handler), stream.get_executor())
    , stream_(stream)
    , buffer_(build_stable_buffer())
    , resolver_(this->get_executor())
  {
    // cache the username in the buffer to avoid having to allocate more space
    buffer_.assign(std::begin(username), std::end(username));

    auto service_string =
    #if BOOST_ASIO_HAS_STRING_VIEW
      service;
    #else
      std::string(std::begin(service), std::end(service));
    #endif

    auto host_string =
    #if BOOST_ASIO_HAS_STRING_VIEW
      hostname;
    #else
      std::string(std::begin(hostname), std::end(hostname));
    #endif

    resolver_.async_resolve(host_string, service_string, std::move(*this));
  }

  #include <boost/asio/yield.hpp>

  // handler overload for resolve step. This is not a coroutine but it may
  // initiate the main coroutine if successful
  void operator()
    (
      error_code ec,
      net::ip::tcp::resolver::results_type candidates
    )
  {
    if(not ec)
    {
      for(auto &&candidate : candidates)
      {
        if(not candidate.endpoint().address().is_v4())
          continue;

        // invoke the main coroutine
        prepare_request(candidate.endpoint());
        return (*this)(ec, 0);
      }

      // could not find an ip4 candidate
      // socks 4 does not understand ipv6

      ec = make_error_code(net::error::host_not_found);
    }

    this->complete_now(ec);
  }

  void
  operator()(
    error_code ec,
    std::size_t /*bytes_transferred*/ = 0)
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

  // return a string in which the allocated data is guaranteed to be at a
  // stable address when the string is moved
  // The implementation intentionally defeats the SBO of the std basic_string
  // template.
  // The returned buffer is associated with a copy of the completion handler's
  // allocator in order to take advantage of any available efficiencies
  auto build_stable_buffer(std::size_t hint = 0) -> string_buffer_type
  {
    auto result = string_buffer_type(this->get_allocator());

    // an empty string's capacity is the maxmimum number of chars it can
    // contain before allocating storage from the associated allocator
    auto min_to_be_stable = result.capacity() + 1;

    result.reserve(std::max(min_to_be_stable, hint));

    return result;
  }

  template<class T, std::size_t byte_count = sizeof(T)>
  static auto as_big_endian_bytes(T value) -> std::array<char, byte_count>
  {
    auto result = std::array<char, byte_count>();

    auto shift = 8 * byte_count;
    for(auto& out : result)
    {
      shift -= 8;
      auto shifted = value >> shift;
      out = static_cast<char>(shifted & T(0xff));
    }

    return result;
  }

  template<class TargetContainer, class SourceContainer>
  auto append_container(TargetContainer &target,
                        SourceContainer const &source) -> TargetContainer &
  {
    target.append(std::begin(source), std::end(source));
    return target;
  }

  // Attempt to build the connect frame in the buffer space.
  // Return an error_code to indicate success
  // @pre the buffer_ must already contain the username paramter
  void
  prepare_request(net::ip::tcp::endpoint endpoint)
  {
    // construct a temporary buffer to build the elements prior to the username
    // this does not have to be stable so we can make use of any SBO.
    // this is a use case for fixed_length_string or similar
    auto temp = string_buffer_type();
    temp += detail::SOCKS_VERSION_4;
    temp += detail::SOCKS_CMD_CONNECT;
    append_container(temp, as_big_endian_bytes(std::int16_t(endpoint.port())));
    append_container(temp, endpoint.address().to_v4().to_bytes());

    // insert the temporary buffer prior to the username which has been cached
    // there
    buffer_.insert(std::begin(buffer_), std::begin(temp), std::end(temp));
    buffer_ += '\x00'; // username is null terminated in SOCKS4
  }

  void prepare_for_response()
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

private:
    // a reference to the stream object passed by the initiator
    Stream&     stream_;

    // A string will associated allocator in which the character data is
    // guaranteed to have a stable address
    string_buffer_type buffer_;

    // The socks4 protocol supports only ip4 addresses, not FQDNs therefore
    // we provide address resolution on behalf of the caller
    net::ip::tcp::resolver resolver_;
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
    string_view service,
    string_view username
  )
  {
    using DecayedHandlerType = typename std::decay<HandlerType>::type;

    socks4_op<AsyncStream, DecayedHandlerType>
    (
      stream,
      std::forward<HandlerType>(handler),
      hostname,
      service,
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
    string_view service,
    string_view username,
    Handler&& handler)
{
  return net::async_initiate<Handler, void(error_code)>
  (
    async_handshake_v4_initiator(),
    handler,
    stream,
    hostname,
    service,
    username
  );
}

} // socks


#endif
