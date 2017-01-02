//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/mutual_ptr.hpp>

#include <beast/unit_test/suite.hpp>

namespace beast {

struct mutual_ptr_test : beast::unit_test::suite
{
    void run() override
    {
    }
};

BEAST_DEFINE_TESTSUITE(mutual_ptr,core,beast);

} // beast
