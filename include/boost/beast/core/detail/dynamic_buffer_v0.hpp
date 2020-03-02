//
// Copyright (c) 2016-2020 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_ASIO_CORE_DETAIL_DYNAMIC_BUFFER_V0_HPP
#define BOOST_ASIO_CORE_DETAIL_DYNAMIC_BUFFER_V0_HPP

#include <boost/asio/buffer.hpp>
#include <boost/beast/core/detail/type_traits.hpp>

namespace boost {
namespace beast {
namespace detail {

template<class Type, class Enable = void>
struct is_dynamic_buffer_v0 : std::false_type
{
};


// A class befriended by models of DynamicBuffer_v0 to allow access to the private
// v1 interface
// DynamicBuffer_v2 adapters can gain access to the v0 dynamic buffers they are adapting
// by using the static members of this class
struct dynamic_buffer_v2_access
{
    template<class DynamicBuffer_v1>
    static
    void
    grow(DynamicBuffer_v1& db1, std::size_t n)
    {
        db1.commit(db1.prepare(n).size());
    }

    template<class DynamicBuffer_v0>
    static
    void
    shrink(DynamicBuffer_v0& db0, std::size_t n)
    {
        db0.shrink_impl(n);
    }

    template<class DynamicBuffer_v0>
    static
    typename DynamicBuffer_v0::mutable_buffers_type
    data(
        DynamicBuffer_v0& db0,
        std::size_t pos,
        std::size_t n)
    {
        return db0.data_impl(pos, n);
    }

    template<class DynamicBuffer_v0>
    static
    typename DynamicBuffer_v0::const_buffers_type
    data(
        DynamicBuffer_v0 const& db0,
        std::size_t pos,
        std::size_t n)
    {
        return db0.data_impl(pos, n);
    }
};

/** Models an object which wraps a reference to a DynamicBuffer_v0 in order to provide
    a DynamicBuffer_v2 interface and behaviour.

    @tparam DynamicBuffer_v0 An object which models a legacy DynamicBuffer_v0, such as a multi_buffer.

    @see buffers_adaptor
    @see flat_buffer
    @see flat_static_buffer
    @see multi_buffer
    @see static_buffer
 */
template<class DynamicBuffer_v0>
struct dynamic_buffer_v0_proxy
{
    BOOST_STATIC_ASSERT(detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value);

    /** Constructor

     Construct a DynamicBuffer_v2 proxy from a reference to a DynamicBuffer_v0.

     @param storage a reference to a model of a DynamicBuffer_v0. The referenced object must survive for at least
            as long as the constructed dynamic_buffer_v0_proxy object and its address mut remain stable.
            Models of DynamicBuffer_V0 include
            - @ref buffers_adaptor
            - @ref flat_buffer
            - @ref flat_static_buffer
            - @ref multi_buffer
            - @ref static_buffer
     */

    dynamic_buffer_v0_proxy(
        DynamicBuffer_v0& storage) noexcept;

    // DynamicBuffer_v2 interface

    /** The type used to represent a sequence of constant buffers that refers to the underlying memory
        of the referenced DynamicBuffer_v0 object.
    */
    using const_buffers_type = typename DynamicBuffer_v0::const_buffers_type;

    /** The type used to represent a sequence of mutable buffers that refers to the underlying memory
        of the referenced DynamicBuffer_v0 object.
    */
    using mutable_buffers_type = typename DynamicBuffer_v0::mutable_buffers_type;

    /** Get the current size of the underlying memory.

     @note The function returns the sizeof of the input sequence of the referenced DynamicBuffer_v0.
           @see data. @see grow. @see shrink.

     @returns The current size of the underlying memory.
    */
    std::size_t
    size() const;

    /** Get the maximum size of the dynamic buffer.

     @returns The allowed maximum size of the underlying memory.
    */
    std::size_t
    max_size() const;

    /** Get the maximum size that the buffer may grow to without triggering reallocation.

     @returns The current capacity.
    */
    std::size_t
    capacity() const;

    /** Consume the specified number of bytes from the beginning of the referenced DynamicBuffer_v0.

     @note If n is greater than the size of the input sequence of the referenced DynamicBuffer_v0,
           the entire input sequence is consumed and no error is issued.
    */
    void
    consume(std::size_t n);

    /** Get a sequence of buffers that represents the underlying memory.

     @param pos Position of the first byte to represent in the buffer sequence.

     @param n The number of bytes to return in the buffer sequence. If the underlying memory is shorter,
              the buffer sequence represents as many bytes as are available.

     @note The returned object is invalidated by any dynamic_buffer_v0_proxy or DynamicBuffer_v2  member
           function that resizes or erases the input sequence of the referenced DynamicBuffer_v2.

     @returns A const_buffers_type containing a sequence of buffers representing the input area
              of the referenced DynamicBuffer_v0.
    */
    const_buffers_type
    data(std::size_t pos, std::size_t n) const;

    /** Get a sequence of buffers that represents the underlying memory.

     @param pos Position of the first byte to represent in the buffer sequence.

     @param n The number of bytes to return in the buffer sequence. If the underlying memory is shorter,
              the buffer sequence represents as many bytes as are available.

     @note The returned object is invalidated by any dynamic_buffer_v0_proxy or DynamicBuffer_v0 member
           function that resizes or erases the input sequence of the referenced DynamicBuffer_v0.

     @returns A mutable_buffers_type containing a sequence of buffers representing the input area
              of the referenced DynamicBuffer_v0.
    */
    mutable_buffers_type
    data(std::size_t pos, std::size_t n);

    /** Grow the underlying memory by the specified number of bytes.

     Resizes the input area of the referenced DynamicBuffer_v0 to accommodate an additional n
     bytes at the end.

     @note The operation is implemented in terms of ``commit(prepare(n).size())`` on the referenced
           DynamicBuffer_v0.

     @except std::length_error If ``size() + n > max_size()``.
    */
    void
    grow(std::size_t n);

    /** Shrink the underlying memory by the specified number of bytes.

     Erases `n` bytes from the end of the input area of the referenced DynamicBuffer_v0.
     If `n` is greater than the current size of the input area, the input areas is emptied.

     @note The operation is implemented in terms of ``commit(prepare(n).size())`` on the referenced
           DynamicBuffer_v0.

    */
    void
    shrink(std::size_t n);

private:

    DynamicBuffer_v0* storage_;
};

/// Convert a v0 DynamicBuffer_v0 reference into a DynamicBuffer_v2 object
template<class DynamicBuffer_v0>
auto
impl_dynamic_buffer(DynamicBuffer_v0& target) ->
typename std::enable_if<
    detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value,
    detail::dynamic_buffer_v0_proxy<DynamicBuffer_v0>>::type;

/// Passthrough conversion for DynamicBuffer_v2 to DynamicBuffer_v2
template<class DynamicBuffer_v2, typename
    std::enable_if<
        boost::asio::is_dynamic_buffer_v2<DynamicBuffer_v2>::value>::type* = nullptr>
auto
impl_dynamic_buffer(DynamicBuffer_v2 buffer) ->
DynamicBuffer_v2
{
    return buffer;
}

/** Determine if `T` is convertible to a DynamicBuffer_v2 via a free function overload of <em>dynamic_buffer</em>.
    @see dynamic_buffer
*/
template<class Type, class>
struct convertible_to_dynamic_buffer_v2;

}
}
}

#include <boost/beast/core/detail/impl/dynamic_buffer_v0.hpp>

#endif

