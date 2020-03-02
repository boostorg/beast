//
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DYNAMIC_BUFFER_HPP
#define BOOST_BEAST_CORE_DYNAMIC_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/buffers_pair.hpp>
#include <boost/beast/core/detail/dynamic_buffer_v0.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <boost/type_traits/make_void.hpp>

namespace boost {
namespace beast {


/** Convert a reference to a DynamicBuffer_v0 into a copyable net.ts dynamic_buffer.

    This function will automatically detect the type of dynamic buffer passed as an argument
    and will return a type modelling DynamicBuffer_v2 which will used the supplied `target` as storage.
    The object returned from this function may be used in any Asio, Net.TS or Beast function which expects
    a DynamicBuffer_v2 argument.

    @tparam DynamicBuffer_v0

    @param target an lvalue reference to a model of DynamicBuffer_v0

    @return An object of unspecified type which models a DynamicBuffer_v2.

    @note The lifetime of the referenced buffer `target` must not end prior to the destruction of the returned object.
    @note Both the returned object and `target` remain valid during the lifetime of this object. The methods `grow`,
          `shrink` and `data` when called on the V2 object will grow, shrink and reference the input area of `target`.

    @see buffers_adaptor
    @see flat_buffer
    @see flat_static_buffer
    @see multi_buffer
    @see static_buffer
 */
template<class DynamicBuffer_v0>
auto
dynamic_buffer(DynamicBuffer_v0& target) ->
#if BOOST_BEAST_DOXYGEN
__see_below__;
#else
typename std::enable_if<
    detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value,
    detail::dynamic_buffer_v0_proxy<DynamicBuffer_v0>>::type;
#endif

}
}

#include <boost/beast/core/impl/dynamic_buffer.hpp>

#endif //BOOST_BEAST_CORE_DYNAMIC_BUFFER_HPP
