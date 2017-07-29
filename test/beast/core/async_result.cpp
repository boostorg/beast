//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/async_result.hpp>

#include <boost/beast/core/error.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <cstdlib>

namespace boost {
namespace beast {
namespace {

struct handler
{
    void operator()(beast::error_code, std::size_t) const;
};

static_assert(detail::is_invocable<
    typename async_result<handler, void(error_code, std::size_t)>::completion_handler_type,
    void(error_code, std::size_t)>::value, "");

static_assert(std::is_same<void,
    typename async_result<handler, void(error_code, std::size_t)>::return_type>::value, "");

static_assert(std::is_constructible<
    async_result<handler,
        void(error_code, std::size_t)>,
    typename async_result<handler,
        void(error_code, std::size_t)
            >::completion_handler_type&>::value, "");

} // (anon-ns)
} // beast
} // boost
