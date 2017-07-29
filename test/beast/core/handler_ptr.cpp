//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/handler_ptr.hpp>

#include <boost/beast/unit_test/suite.hpp>
#include <exception>
#include <utility>

namespace boost {
namespace beast {

class handler_ptr_test : public beast::unit_test::suite
{
public:
    struct handler
    {
        handler() = default;
        handler(handler const&) = default;

        void
        operator()(bool& b) const
        {
            b = true;
        }
    };

    struct T
    {
        T(handler&)
        {
        }

        ~T()
        {
        }
    };

    struct U
    {
        U(handler&)
        {
            throw std::exception{};
        }
    };

    void
    run() override
    {
        handler h;
        handler_ptr<T, handler> p1{h};
        handler_ptr<T, handler> p2{p1};
        try
        {
            handler_ptr<U, handler> p3{h};
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
        catch(...)
        {
            fail();
        }
        handler_ptr<T, handler> p4{std::move(h)};
        bool b = false;
        p4.invoke(std::ref(b));
        BEAST_EXPECT(b);
    }
};

BOOST_BEAST_DEFINE_TESTSUITE(handler_ptr,core,beast);

} // beast
} // boost
