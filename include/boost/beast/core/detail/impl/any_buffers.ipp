//
// Copyright (c) 2016-2021 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core/detail/any_buffers.hpp>

namespace boost {
namespace beast {
namespace detail {

template<class ValueType>
std::size_t
any_buffers_impl_base<ValueType>::addref() noexcept
{
    return ++refs;
}

template<class ValueType>
std::size_t
any_buffers_impl_base<ValueType>::release() noexcept
{
    auto count = --refs;
    if (count == 0)
        destroy();
    return count;
}

// --------------------------------------------------------------


template<bool IsConst>
any_buffers<IsConst>::any_buffers(any_buffers const& r)
: impl_(r.impl_)
{
    if (impl_)
        impl_->addref();
}

template<bool IsConst>
any_buffers<IsConst>::any_buffers(any_buffers && r) noexcept
: impl_(boost::exchange(r.impl_, nullptr))
{
}

template<bool IsConst>
any_buffers<IsConst>::~any_buffers()
{
    if (impl_)
        impl_->release();
}

template class any_buffers<true>;
template class any_buffers<false>;

}
}
}