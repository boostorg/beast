//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_HTTP_GENERATOR_HPP
#define BOOST_BEAST_HTTP_IMPL_HTTP_GENERATOR_HPP

#include <boost/beast/http/http_generator.hpp>

namespace boost {
namespace beast {
namespace http {

template <bool isRequest, class Body, class Fields>
struct http_generator::generator_impl
    : http_generator::impl_base
{
    http::message<isRequest, Body, Fields> m_;
    http::serializer<isRequest, Body, Fields> sr_;
    std::array<net::const_buffer, 16> buf_;

    explicit generator_impl(
        http::message<isRequest, Body, Fields>&& m)
        : m_(std::move(m))
        , sr_(m_)
    {
    }

    struct visit
    {
        generator_impl* this_;
        const_buffers_type& cb_;

        template <class ConstBufferSequence>
        void
        operator()(error_code&,
                   ConstBufferSequence const& buffers)
        {
            auto it = net::buffer_sequence_begin(buffers);
            auto const end = net::buffer_sequence_end(buffers);

            std::size_t n = 0;
            while (it != end && n < this_->buf_.size())
                this_->buf_[n++] = *it++;
            cb_ = { this_->buf_.data(), n };
        }
    };

    const_buffers_type
    prepare(error_code& ec) override
    {
        if (sr_.is_done())
            return {};
        const_buffers_type cb;
        sr_.next(ec, visit{ this, cb });
        return cb;
    }

    void
    consume(std::size_t n) override
    {
        sr_.consume(n);
    }

    bool
    keep_alive() const noexcept override
    {
        return m_.keep_alive();
    }
};

} // namespace http
} // namespace beast
} // namespace boost

#endif // BOOST_BEAST_HTTP_IMPL_HTTP_GENERATOR_HPP
