//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_ERROR_HPP
#define BOOST_BEAST_IMPL_ERROR_HPP

#include <type_traits>

namespace boost {
namespace system {
template<>
struct is_error_code_enum<beast::error>
{
    static bool const value = true;
};

template<>
struct is_error_condition_enum<beast::condition>
{
    static bool const value = true;
};

} // system
} // boost

namespace boost {
namespace beast {
namespace detail {

class error_codes : public error_category
{
public:
    BOOST_BEAST_DECL
    const char*
    name() const noexcept override;

    BOOST_BEAST_DECL
    std::string
    message(int ev) const override;

    BOOST_BEAST_DECL
    error_condition
    default_error_condition(int ev) const noexcept override;
};

class error_conditions : public error_category
{
public:
    BOOST_BEAST_DECL
    const char*
    name() const noexcept override;

    BOOST_BEAST_DECL
    std::string
    message(int cv) const override;
};

} // detail

BOOST_BEAST_DECL
error_code
make_error_code(error e);

BOOST_BEAST_DECL
error_condition
make_error_condition(condition c);

} // beast
} // boost

#include <boost/beast/core/impl/error.ipp>

#endif
