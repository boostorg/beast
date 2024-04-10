//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/saved_handler.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
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
        bool failed_ = true;
        bool throw_on_move_ = false;
    
    public:
        handler() = default;
        handler(handler const&) = delete;
        handler& operator=(handler&&) = delete;
        handler& operator=(handler const&) = delete;

        ~handler()
        {
            BEAST_EXPECT(! failed_);
        }

        handler(handler&& other)
            : failed_(boost::exchange(
                other.failed_, false))
        {
            if(throw_on_move_)
                throw std::bad_alloc{};
        }

        void
        operator()(system::error_code = {})
        {
            failed_ = false;
        }
    };

    class unhandler
    {
        bool invoked_ = false;
   
    public:
        unhandler() = default;
        unhandler(unhandler const&) = delete;
        unhandler& operator=(unhandler&&) = delete;
        unhandler& operator=(unhandler const&) = delete;

        ~unhandler()
        {
            BEAST_EXPECT(! invoked_);
        }

        unhandler(unhandler&& other)
            : invoked_(boost::exchange(
                other.invoked_, false))
        {
        }

        void
        operator()(system::error_code = {})
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
        operator()(system::error_code = {})
        {
        }
    };

    void
    testSavedHandler()
    {
        {
            saved_handler sh;
            BEAST_EXPECT(! sh.has_value());

            sh.emplace(handler{});
            BEAST_EXPECT(sh.has_value());
            sh.invoke();
            BEAST_EXPECT(! sh.has_value());

            sh.emplace(handler{}, std::allocator<char>{});
            BEAST_EXPECT(sh.has_value());
            sh.invoke();
            BEAST_EXPECT(! sh.has_value());

            sh.emplace(unhandler{});
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
    testSavedHandlerCancellation()
    {
        {
            net::cancellation_signal sig;          

            saved_handler sh;
            BEAST_EXPECT(! sh.has_value());

            sh.emplace(
                    net::bind_cancellation_slot(
                        sig.slot(), handler{}));
            BEAST_EXPECT(sh.has_value());
            BEAST_EXPECT(sig.slot().has_handler());
            sig.emit(net::cancellation_type::all);
            BEAST_EXPECT(! sh.has_value());
            BEAST_EXPECT(!sig.slot().has_handler());


            sh.emplace(
                    net::bind_cancellation_slot(
                        sig.slot(), handler{}));
            BEAST_EXPECT(sh.has_value());
            BEAST_EXPECT(sig.slot().has_handler());
            sig.emit(net::cancellation_type::total);
            BEAST_EXPECT(sh.has_value());
            BEAST_EXPECT(sig.slot().has_handler());
            sig.emit(net::cancellation_type::terminal);
            BEAST_EXPECT(! sh.has_value());
            BEAST_EXPECT(!sig.slot().has_handler());

            sh.emplace(
                    net::bind_cancellation_slot(
                        sig.slot(), handler{}), 
                        net::cancellation_type::total);
            BEAST_EXPECT(sh.has_value());
            BEAST_EXPECT(sig.slot().has_handler());
            sig.emit(net::cancellation_type::total);
            BEAST_EXPECT(! sh.has_value());
            BEAST_EXPECT(!sig.slot().has_handler());

            {
                saved_handler sh_inner;
                sh_inner.emplace(
                    net::bind_cancellation_slot(
                        sig.slot(), handler{}));

                sh = std::move(sh_inner);
            }
            BEAST_EXPECT(sh.has_value());
            BEAST_EXPECT(sig.slot().has_handler());
            sig.emit(net::cancellation_type::all);
            BEAST_EXPECT(! sh.has_value());
            BEAST_EXPECT(!sig.slot().has_handler());

        }
        {
            saved_handler sh;
            net::cancellation_signal sig;          

            try
            { 
                sh.emplace(
                    net::bind_cancellation_slot(
                        sig.slot(), 
                        throwing_handler{}));
                fail();
            }
            catch(std::exception const&)
            {
                pass();
            }
            BEAST_EXPECT(!sig.slot().has_handler());
            BEAST_EXPECT(! sh.has_value());
        }
    }

    void
    run() override
    {
        testSavedHandler();
        testSavedHandlerCancellation();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,saved_handler);

} // beast
} // boost
