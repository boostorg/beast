//
// Copyright (c) 2019 Damian Jarek(damian.jarek93@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_TEMPORARY_BUFFER_HPP
#define BOOST_BEAST_DETAIL_TEMPORARY_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/string.hpp>

#include <memory>

namespace boost {
namespace beast {
namespace detail {

struct temporary_buffer
{
    temporary_buffer() = default;

    temporary_buffer(temporary_buffer const&) = delete;
    temporary_buffer(temporary_buffer&&) = delete;

    temporary_buffer& operator=(temporary_buffer const&) = delete;
    temporary_buffer& operator=(temporary_buffer&&) = delete;

    ~temporary_buffer() noexcept
    {
        deallocate(data_);
    }

    BOOST_BEAST_DECL
    void
    append(string_view sv);

    BOOST_BEAST_DECL
    void
    append(string_view sv1, string_view sv2);

    string_view
    view() const noexcept
    {
        return {data_, size_};
    }

    bool
    empty() const noexcept
    {
        return size_ == 0;
    }

private:
    BOOST_BEAST_DECL
    void
    unchecked_append(string_view sv);

    BOOST_BEAST_DECL
    void
    grow(std::size_t sv_size);

    void
    deallocate(char* data) noexcept
    {
        if (data != buffer_)
            delete[] data;
    }

    char buffer_[4096];
    char* data_ = buffer_;
    std::size_t capacity_ = sizeof(buffer_);
    std::size_t size_ = 0;
};

} // detail
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/core/detail/impl/temporary_buffer.ipp>
#endif

#endif
