//
// Copyright (c) 2019 Damian Jarek(damian.jarek93@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_IMPL_TEMPORARY_BUFFER_IPP
#define BOOST_BEAST_DETAIL_IMPL_TEMPORARY_BUFFER_IPP

#include <boost/beast/core/detail/temporary_buffer.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/core/exchange.hpp>

#include <memory>
#include <cstring>

namespace boost {
namespace beast {
namespace detail {


void
temporary_buffer::append(string_view sv)
{
    grow(sv.size());
    unchecked_append(sv);
}

void
temporary_buffer::append(string_view sv1, string_view sv2)
{
    grow(sv1.size() + sv2.size());
    unchecked_append(sv1);
    unchecked_append(sv2);
}

void
temporary_buffer::unchecked_append(string_view sv)
{
    auto n = sv.size();
    std::memcpy(&data_[size_], sv.data(), n);
    size_ += n;
}

void
temporary_buffer::grow(std::size_t sv_size)
{
    if (capacity_ - size_ >= sv_size)
    {
        return;
    }

    auto const new_cap = (sv_size + size_) * 2u;
    BOOST_ASSERT(!detail::sum_exceeds(sv_size, size_, new_cap));
    char* const p = new char[new_cap];
    std::memcpy(p, data_, size_);
    deallocate(boost::exchange(data_, p));
    capacity_ = new_cap;
}
} // detail
} // beast
} // boost

#endif
