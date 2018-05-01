//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_FAIL_COUNTER_HPP
#define BOOST_BEAST_TEST_FAIL_COUNTER_HPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/experimental/test/error.hpp>
#include <boost/throw_exception.hpp>

namespace boost {
namespace beast {
namespace test {

/** A countdown to simulated failure.

    On the Nth operation, the class will fail with the specified
    error code, or the default error code of @ref error::test_failure.
*/
class fail_counter
{
    std::size_t n_;
    std::size_t i_ = 0;
    error_code ec_;

public:
    fail_counter(fail_counter&&) = default;

    /** Construct a counter.

        @param The 0-based index of the operation to fail on or after.
    */
    explicit
    fail_counter(std::size_t n,
            error_code ev = make_error_code(error::test_failure))
        : n_(n)
        , ec_(ev)
    {
    }

    /// Returns the fail index
    std::size_t
    count() const
    {
        return n_;
    }

    /// Throw an exception on the Nth failure
    void
    fail()
    {
        if(i_ < n_)
            ++i_;
        if(i_ == n_)
            BOOST_THROW_EXCEPTION(system_error{ec_});
    }

    /// Set an error code on the Nth failure
    bool
    fail(error_code& ec)
    {
        if(i_ < n_)
            ++i_;
        if(i_ == n_)
        {
            ec = ec_;
            return true;
        }
        ec.assign(0, ec.category());
        return false;
    }
};

} // test
} // beast
} // boost

#endif
