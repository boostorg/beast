//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/saved_handler.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <stdexcept>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------

class saved_handler_test : public unit_test::suite
{
public:
    class handler
    {
        unit_test::suite& s_;
        bool failed_ = true;
        bool throw_on_move_ = false;
    
    public:
        handler(handler const&) = delete;
        handler& operator=(handler&&) = delete;
        handler& operator=(handler const&) = delete;

        ~handler()
        {
            s_.BEAST_EXPECT(! failed_);
        }

        explicit
        handler(unit_test::suite& s)
            : s_(s)
        {
        }

        handler(handler&& other)
            : s_(other.s_)
            , failed_(boost::exchange(
                other.failed_, false))
        {
            if(throw_on_move_)
                throw std::bad_alloc{};
        }

        void
        operator()()
        {
            failed_ = false;
        }
    };

    class unhandler
    {
        unit_test::suite& s_;
        bool invoked_ = false;
   
    public:
        unhandler(unhandler const&) = delete;
        unhandler& operator=(unhandler&&) = delete;
        unhandler& operator=(unhandler const&) = delete;

        ~unhandler()
        {
            s_.BEAST_EXPECT(! invoked_);
        }

        explicit
        unhandler(unit_test::suite& s)
            : s_(s)
        {
        }

        unhandler(unhandler&& other)
            : s_(other.s_)
            , invoked_(boost::exchange(
                other.invoked_, false))
        {
        }

        void
        operator()()
        {
            invoked_ = true;
        }
    };

    struct throwing_handler
    {
        throwing_handler() = default;

        throwing_handler(throwing_handler&&)
        {
            BOOST_THROW_EXCEPTION(std::exception{});
        }

        void
        operator()()
        {
        }
    };

    void
    testSavedHandler()
    {
        {
            saved_handler sh;
            BEAST_EXPECT(! sh.has_value());

            sh.emplace(handler{*this});
            BEAST_EXPECT(sh.has_value());
            sh.invoke();
            BEAST_EXPECT(! sh.has_value());

            sh.emplace(handler{*this}, std::allocator<char>{});
            BEAST_EXPECT(sh.has_value());
            sh.invoke();
            BEAST_EXPECT(! sh.has_value());

            sh.emplace(unhandler{*this});
            BEAST_EXPECT(sh.has_value());
        }

        {
            saved_handler sh;
            try
            {
                sh.emplace(throwing_handler{});
                fail();
            }
            catch(std::exception const&)
            {
                pass();
            }
            BEAST_EXPECT(! sh.has_value());
        }
    }

    void
    run() override
    {
        testSavedHandler();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,saved_handler);

} // beast
} // boost
