//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TYPE_TRAITS_HPP
#define BOOST_BEAST_TYPE_TRAITS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/file_base.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <type_traits>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------
//
// Handler concepts
//
//------------------------------------------------------------------------------

/** Determine if `T` meets the requirements of @b CompletionHandler.

    This trait checks whether a type meets the requirements for a completion
    handler, and is also callable with the specified signature.
    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example

    Use with `static_assert`:

    @code
    struct handler
    {
        void operator()(error_code&);
    };

    static_assert(is_completion_handler<handler, void(error_code&)>::value,
        "Not a completion handler");
    @endcode
*/
template<class T, class Signature>
#if BOOST_BEAST_DOXYGEN
using is_completion_handler = std::integral_constant<bool, ...>;
#else
using is_completion_handler = std::integral_constant<bool,
    std::is_move_constructible<typename std::decay<T>::type>::value &&
    detail::is_invocable<T, Signature>::value>;
#endif

} // beast
} // boost

#endif
