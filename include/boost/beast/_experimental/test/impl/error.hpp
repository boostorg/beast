//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_IMPL_ERROR_HPP
#define BOOST_BEAST_TEST_IMPL_ERROR_HPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/core/string.hpp>
#include <type_traits>

namespace boost {
namespace system {
template<>
struct is_error_code_enum<
    boost::beast::test::error>
        : std::true_type
{
};
} // system
} // boost

namespace boost {
namespace beast {
namespace test {

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

} // detail

BOOST_BEAST_DECL
error_code
make_error_code(error e) noexcept;

} // test
} // beast
} // boost

#include <boost/beast/_experimental/test/impl/impl/error.hpp>

#endif
