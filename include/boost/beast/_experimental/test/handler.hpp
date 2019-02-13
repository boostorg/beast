//
// Copyright (c) 2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_HANDLER_HPP
#define BOOST_BEAST_TEST_HANDLER_HPP

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/core/exchange.hpp>
#include <boost/optional.hpp>

namespace boost {
namespace beast {
namespace test {

/** A CompletionHandler used for testing.

    This completion handler is used by tests to ensure correctness
    of behavior. It is designed as a single type to reduce template
    instantiations, with configurable settings through constructor
    arguments. Typically this type will be used in type lists and
    not instantiated directly; instances of this class are returned
    by the helper functions listed below.

    @see @ref success_handler, @ref fail_handler, @ref any_handler
*/
class handler
{
    boost::optional<error_code> ec_;
    bool pass_ = false;

public:
    handler() = default;

    explicit
    handler(error_code ec)
        : ec_(ec)
    {
    }

    explicit
    handler(boost::none_t)
    {
    }

    handler(handler&& other)
        : ec_(other.ec_)
        , pass_(boost::exchange(other.pass_, true))
    {
    }

    ~handler()
    {
        BEAST_EXPECT(pass_);
    }

    template<class... Args>
    void
    operator()(error_code ec, Args&&...)
    {
        BEAST_EXPECT(! pass_); // can't call twice
        BEAST_EXPECTS(! ec_ || ec == *ec_,
            ec.message());
        pass_ = true;
    }

    void
    operator()()
    {
        BEAST_EXPECT(! pass_); // can't call twice
        BEAST_EXPECT(! ec_);
        pass_ = true;
    }

    template<class Arg0, class... Args,
        class = typename std::enable_if<
            ! std::is_convertible<Arg0, error_code>::value>::type>
    void
    operator()(Arg0&&, Args&&...)
    {
        BEAST_EXPECT(! pass_); // can't call twice
        BEAST_EXPECT(! ec_);
        pass_ = true;
    }
};

/** Return a test CompletionHandler which requires success.
    
    The returned handler can be invoked with any signature whose
    first parameter is an `error_code`. The handler fails the test
    if:

    @li The handler is destroyed without being invoked, or

    @li The handler is invoked with a non-successful error code.
*/
inline
handler
success_handler() noexcept
{
    return handler(error_code{});
}

/** Return a test CompletionHandler which requires invocation.

    The returned handler can be invoked with any signature.
    The handler fails the test if:

    @li The handler is destroyed without being invoked.
*/
inline
handler
any_handler() noexcept
{
    return handler(boost::none);
}

/** Return a test CompletionHandler which requires a specific error code.

    This handler can be invoked with any signature whose first
    parameter is an `error_code`. The handler fails the test if:

    @li The handler is destroyed without being invoked.

    @li The handler is invoked with an error code different from
    what is specified.

    @param ec The error code to specify.
*/
inline
handler
fail_handler(error_code ec) noexcept
{
    return handler(ec);
}

} // test
} // beast
} // boost

#endif
