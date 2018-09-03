//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_URI_ERROR_HPP
#define BOOST_BEAST_CORE_URI_ERROR_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>

namespace boost {
namespace beast {
namespace uri {

enum class error
{
    /// An input did not match a structural element (soft error)
    mismatch = 1,

    /// A syntax error occurred
    syntax,

    /// The parser encountered an invalid input
    invalid
};

} // uri
} // beast
} // boost

namespace boost {
namespace system {
template<>
struct is_error_code_enum<boost::beast::uri::error>
{
    static bool const value = true;
};
} // system
} // boost

namespace boost {
namespace beast {
namespace uri {

namespace detail {

class uri_error_category : public error_category
{
public:
    const char*
    name() const noexcept override
    {
        return "beast.http.uri";
    }

    std::string
    message(int ev) const override
    {
        switch(static_cast<uri::error>(ev))
        {
        case error::mismatch: return "mismatched element";
        case error::syntax: return "syntax error";
        case error::invalid: return "invalid input";
        default:
            return "beast.http.uri error";
        }
    }

    error_condition
    default_error_condition(
        int ev) const noexcept override
    {
        return error_condition{ev, *this};
    }

    bool
    equivalent(int ev,
        error_condition const& condition
            ) const noexcept override
    {
        return condition.value() == ev &&
            &condition.category() == this;
    }

    bool
    equivalent(error_code const& error,
        int ev) const noexcept override
    {
        return error.value() == ev &&
            &error.category() == this;
    }
};

inline
error_category const&
get_uri_error_category()
{
    static uri_error_category const cat{};
    return cat;
}

} // detail

inline
error_code
make_error_code(error ev)
{
    return error_code{
        static_cast<std::underlying_type<error>::type>(ev),
            detail::get_uri_error_category()};
}

} // uri
} // beast
} // boost

#endif
