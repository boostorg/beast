//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_ERROR_IPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_ERROR_IPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/core/string.hpp>

namespace boost {

namespace beast {
namespace websocket {
enum class error;
} // websocket
} // beast

namespace system {
template<>
struct is_error_code_enum<beast::websocket::error>
{
    static bool const value = true;
};
} // system

namespace beast {
namespace websocket {
namespace detail {

class error_codes : public error_category
{
    template<class = void>
    string_view
    get_message(error ev) const;

public:
    const char*
    name() const noexcept override
    {
        return "boost.beast.websocket";
    }

    std::string
    message(int ev) const override
    {
        return get_message(static_cast<error>(ev)).to_string();
    }
};

} // detail

inline
error_code
make_error_code(error e)
{
    static detail::error_codes const cat{};
    return error_code{static_cast<
        std::underlying_type<error>::type>(e), cat};
}

} // websocket
} // beast

} // boost

#endif
