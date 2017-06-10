//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/string_param.hpp>

#include <beast/unit_test/suite.hpp>
#include <beast/core/detail/type_traits.hpp>

namespace beast {

class string_param_test : public unit_test::suite
{
public:
    struct nop {};

    static_assert(! std::is_constructible<string_param, nop>::value, "");

    void
    check(string_param const& v, string_view s)
    {
        BEAST_EXPECT(v.str() == s);
    }

    void
    run() override
    {
        check(std::string("hello"), "hello");
        check("xyz", "xyz");
        check(1, "1");
        check(12, "12");
        check(123, "123");
        check(1234, "1234");
        check(12345, "12345");
    }
};

BEAST_DEFINE_TESTSUITE(string_param,core,beast);

} // beast
