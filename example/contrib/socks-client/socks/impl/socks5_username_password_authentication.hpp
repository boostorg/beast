//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
// Copyright (c) 2019 jackarain (hodges dot r at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_SOCKS_IMPL_ASYNC_SOCKS5_USERNAME_PASSWORD_AUTHENTICATION_HPP
#define BOOST_BEAST_EXAMPLE_SOCKS_IMPL_ASYNC_SOCKS5_USERNAME_PASSWORD_AUTHENTICATION_HPP

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

namespace socks
{


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
struct socks5_username_password_op
: base_type
, boost::asio::coroutine
{
  using allocator_type = typename base_type::allocator_type;

  // Rebind the composed operation's allocator type to allow the allocation of chars
  using char_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<char>;

  // build a new string type that shares the composed operation's allocator
  using string_buffer_type = std::basic_string<char, std::char_traits<char>, char_allocator_type>;

  socks5_username_password_op(
    Stream& stream, 
    string_view username, 
    string_view password, 
    Handler handler)
    : base_type(std::move(handler), stream.get_executor())
    , stream_(stream)
    , buffer_(make_stable_buffer(request_size_hint(username, password)))
  {
    auto ec = build_request(username, password);
    if (ec)
    {
      this->complete(false, ec);
    }
    else
    {
      (*this)(ec);
    }
  }

#include <boost/asio/yield.hpp>

  void
  operator()(
    error_code ec = error_code(),
    std::size_t bytes_transferred = 0
  )
  {
    reenter(this)
    {
      yield net::async_write(stream_, net::buffer(buffer_), std::move(*this));
      if (ec) this->complete_now(ec);

      prepare_receive_response();
      yield net::async_read(stream_, net::buffer(buffer_), std::move(*this));
      
      return this->complete_now(ec ? ec : decode_response());
    }
  }

#include <boost/asio/unyield.hpp>

private:

  auto 
  static request_size_hint(string_view username, string_view password)
  -> std::size_t
  {
    return 3 + username.size() + password.size();
  }

  auto
  build_request(string_view username, string_view password)
  -> error_code
  {
    auto error = error_code();

    if (username.size() > 255 or password.size() > 255)
    {
      error = net::error::invalid_argument; 
    }
    else
    {
      buffer_.clear();
      buffer_.push_back('\x01');
      buffer_.push_back(std::uint8_t(username.size()));
      buffer_.append(std::begin(username), std::end(username));
      buffer_.push_back(std::uint8_t(password.size()));
      buffer_.append(std::begin(password), std::end(password));
    }

    return error;
  }

  auto
  make_stable_buffer(std::size_t required_capacity = 0)
  -> string_buffer_type
  {
    auto buffer = string_buffer_type(this->get_allocator());

    auto min_stable = buffer.capacity() + 1;

    buffer.reserve(std::max(required_capacity, min_stable));

    return buffer;
  }

  auto
  prepare_receive_response()
  -> void
  {
    buffer_.resize(2);
  }

  auto
  decode_response()
  -> error_code
  {
    auto error = error_code();

    if (buffer_[0] != '\x01') 
      error = make_error_code(boost::beast::errc::protocol_error);

    if (not error and buffer_[1] != '\x01')
      error = error::socks_authentication_error;

    return error;
  }

  Stream& stream_;
  string_buffer_type buffer_;
};

struct async_socks5_auth_username_password_initiator
{
  template<class Stream, class Handler>
  void operator()(
    Handler&& handler,
    Stream& stream, 
    string_view username,
    string_view password)
  {
    using DecayedHandler = typename std::decay<Handler>::type;

    socks5_username_password_op<Stream, DecayedHandler>(
      stream, 
      username, 
      password, 
      std::forward<Handler>(handler)
    );
  }
};

template<class Stream, class Handler>
auto async_socks5_auth_username_password(
  Stream& stream, 
  string_view username,
  string_view password,
  Handler&& handler)
-> BOOST_BEAST_ASYNC_RESULT1(Handler)
{
  net::async_initiate<Handler, void(error_code)>
  (
    async_socks5_auth_username_password_initiator(),
    handler,
    stream,
    username,
    password
  );
}
}
#endif
