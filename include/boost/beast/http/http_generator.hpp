//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//
#ifndef BOOST_BEAST_HTTP_HTTP_GENERATOR_HPP
#define BOOST_BEAST_HTTP_HTTP_GENERATOR_HPP

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/beast/core/span.hpp>

namespace boost {
namespace beast {
namespace http {

class http_generator
{
public:
    using const_buffers_type = span<net::const_buffer>;

    template <bool isRequest, class Body, class Fields>
    http_generator(
        http::message<isRequest, Body, Fields>&& m)
        : impl_(boost::make_shared<
                generator_impl<isRequest, Body, Fields>>(
              std::move(m)))
    {
    }

    const_buffers_type prepare(error_code& ec)   { return impl_->prepare(ec);      } 
    void consume(std::size_t n)                  { impl_->consume(n);              } 
    bool keep_alive() const noexcept             { return impl_->keep_alive();     } 

private:
    struct impl_base
    {
        virtual ~impl_base() = default;

        virtual const_buffers_type prepare(error_code& ec) = 0;

        virtual void consume(std::size_t n) = 0;

        virtual bool keep_alive() const noexcept = 0;
    };

    boost::shared_ptr<impl_base> impl_;

    template <bool isRequest, class Body, class Fields>
    struct generator_impl;
};

} // namespace http
} // namespace beast
} // namespace boost

#include <boost/beast/http/impl/http_generator.hpp>

#endif // BOOST_BEAST_HTTP_HTTP_GENERATOR_HPP
