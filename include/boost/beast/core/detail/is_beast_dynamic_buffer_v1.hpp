//
// Copyright (c) 2016-2020 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_IS_BEAST_DYNAMIC_BUFFER_V1_HPP
#define BOOST_BEAST_CORE_DETAIL_IS_BEAST_DYNAMIC_BUFFER_V1_HPP

#include <boost/beast/core/detail/config.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

// A traits class metafunction returning true/false type indicating whether the given DynamicBuffer class
// has been marked as having a beast-style v1 dynamic buffer interface

template<class DynamicBuffer>
struct is_beast_dynamic_buffer_v1 : std::false_type {};

} // detail
} // beast
} // boost


#endif // BOOST_BEAST_CORE_DETAIL_IS_BEAST_DYNAMIC_BUFFER_V1_HPP