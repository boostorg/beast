//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DYNAMIC_BUFFER_REF_HPP
#define BOOST_BEAST_CORE_DYNAMIC_BUFFER_REF_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <cstdlib>
#include <type_traits>

namespace boost {
namespace beast {

/** A lightweight reference to a dynamic buffer.

    Objects of this type meet the requirements of <em>DynamicBuffer</em>.
    This is the wrapper returned by the function @ref dynamic_buffer_ref.

    @see dynamic_buffer_ref
*/
template<class DynamicBuffer>
class dynamic_buffer_ref_wrapper
#if ! BOOST_BEAST_DOXYGEN
{
    static_assert(net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");

    DynamicBuffer& b_;

public:
    using const_buffers_type = typename
        DynamicBuffer::const_buffers_type;

    using mutable_buffers_type = typename
        DynamicBuffer::mutable_buffers_type;

    dynamic_buffer_ref_wrapper(
        dynamic_buffer_ref_wrapper&&) = default;

    dynamic_buffer_ref_wrapper(
        dynamic_buffer_ref_wrapper const&) = default;

    explicit
    dynamic_buffer_ref_wrapper(
        DynamicBuffer& b) noexcept
        : b_(b)
    {
    }

    std::size_t
    size() const noexcept
    {
        return b_.size();
    }

    std::size_t
    max_size() const noexcept
    {
        return b_.max_size();
    }

    std::size_t
    capacity() const noexcept
    {
        return b_.capacity();
    }

    const_buffers_type
    data() const noexcept
    {
        return b_.data();
    }

    mutable_buffers_type
    prepare(std::size_t n)
    {
        return b_.prepare(n);
    }

    void
    commit(std::size_t n)
    {
        b_.commit(n);
    }

    void
    consume(std::size_t n)
    {
        b_.consume(n);
    }
}
#endif
;

/** Return a non-owning reference to a dynamic buffer.

    This function returns a wrapper which holds a reference to the
    passed dynamic buffer. The wrapper meets the requirements of
    <em>DynamicBuffer</em>, allowing its use in Networking algorithms
    which want to take ownership of the dynamic buffer. Since Beast
    dynamic buffers are true storage types, they cannot be used directly
    with functions that take ownership of the dynamic buffer.

    @par Example
    This function reads a line of text from a stream into a
    @ref beast::basic_flat_buffer, using the net function `async_read_until`.
    @code
    template <class SyncReadStream>
    std::size_t read_line (SyncReadStream& stream, flat_buffer& buffer)
    {
        return net::read_until(stream, dynamic_buffer_ref(buffer), "\r\n");
    }
    @endcode

    @param buffer The dynamic buffer to wrap. Ownership of the buffer is
    not transferred, the caller is still responsible for managing the
    lifetime of the original object.

    @return A wrapper meeting the requirements of <em>DynamicBuffer</em>
    which references the original dynamic buffer.

    @see dynamic_buffer_ref_wrapper
*/
template<class DynamicBuffer>
dynamic_buffer_ref_wrapper<DynamicBuffer>
dynamic_buffer_ref(DynamicBuffer& buffer) noexcept
{
    static_assert(net::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    return dynamic_buffer_ref_wrapper<DynamicBuffer>(buffer);
}

} // beast
} // boost

#endif
