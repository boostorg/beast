//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/error.hpp>

#include <boost/beast/unit_test/suite.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

class error_test : public unit_test::suite
{
public:
    void check(error e)
    {
        auto const ec = make_error_code(e);
        ec.category().name();
        BEAST_EXPECT(! ec.message().empty());
#if 0
        BEAST_EXPECT(std::addressof(ec.category()) ==
            std::addressof(detail::get_error_category()));
        BEAST_EXPECT(detail::get_error_category().equivalent(
            static_cast<std::underlying_type<error>::type>(e),
                ec.category().default_error_condition(
                    static_cast<std::underlying_type<error>::type>(e))));
        BEAST_EXPECT(detail::get_error_category().equivalent(
            ec, static_cast<std::underlying_type<error>::type>(e)));
#endif
    }

    void check(error e, condition c)
    {
        BEAST_EXPECT(error_code{e} == c);
    }

    void run() override
    {
        check(error::closed);
        check(error::failed);
        check(error::handshake_failed);
        check(error::buffer_overflow);
        check(error::partial_deflate_block);

        check(error::handshake_failed, condition::handshake_failed);
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,error);

} // websocket
} // beast
} // boost
