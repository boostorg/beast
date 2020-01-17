//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_V1_DYNAMIC_STRING_BUFFER_HPP
#define BOOST_BEAST_TEST_V1_DYNAMIC_STRING_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <string>
#include <cstddef>

namespace boost {
namespace beast {

// copied directly from boost asio 1.72 and edited as-if NO_DYNAMIC_BUFFER_V1
struct v1_dynamic_string_buffer
{
    typedef net::const_buffer const_buffers_type;
    typedef net::mutable_buffer mutable_buffers_type;

    explicit v1_dynamic_string_buffer(
        std::string &s,
        std::size_t maximum_size =
        (std::numeric_limits<std::size_t>::max)()) BOOST_ASIO_NOEXCEPT
    : string_(s)
    , size_((std::numeric_limits<std::size_t>::max)())
    , max_size_(maximum_size)
    {
    }

    std::size_t size() const BOOST_ASIO_NOEXCEPT
    {
        if (size_ != (std::numeric_limits<std::size_t>::max)())
            return size_;
        return (std::min)(string_.size(), max_size());
    }

    std::size_t max_size() const BOOST_ASIO_NOEXCEPT
    {
        return max_size_;
    }

    std::size_t capacity() const BOOST_ASIO_NOEXCEPT
    {
        return (std::min)(string_.capacity(), max_size());
    }

    const_buffers_type data() const BOOST_ASIO_NOEXCEPT
    {
        return const_buffers_type(boost::asio::buffer(string_, size_));
    }

    mutable_buffers_type prepare(std::size_t n)
    {
        if (size() > max_size() || max_size() - size() < n)
        {
            std::length_error ex("dynamic_string_buffer too long");
            boost::asio::detail::throw_exception(ex);
        }

        if (size_ == (std::numeric_limits<std::size_t>::max)())
            size_ = string_.size(); // Enable v1 behaviour.

        string_.resize(size_ + n);

        return boost::asio::buffer(boost::asio::buffer(string_) + size_, n);
    }

    void commit(std::size_t n)
    {
        size_ += (std::min)(n, string_.size() - size_);
        string_.resize(size_);
    }

    void consume(std::size_t n)
    {
        if (size_ != (std::numeric_limits<std::size_t>::max)())
        {
            std::size_t consume_length = (std::min)(n, size_);
            string_.erase(0, consume_length);
            size_ -= consume_length;
            return;
        }
        string_.erase(0, n);
    }

private:
    std::string& string_;
    std::size_t size_;
    const std::size_t max_size_;
};

} // beast
} // boost

#endif // BOOST_BEAST_TEST_V1_DYNAMIC_STRING_BUFFER_HPP