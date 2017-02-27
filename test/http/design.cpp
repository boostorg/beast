//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/unit_test/suite.hpp>

namespace beast {
namespace http {

class design_test : public beast::unit_test::suite
{
public:
    void
    run() override
    {
    }
};

BEAST_DEFINE_TESTSUITE(design,http,beast);

} // http
} // beast
