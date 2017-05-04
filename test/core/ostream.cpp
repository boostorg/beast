//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/ostream.hpp>

#include <beast/core/streambuf.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/lexical_cast.hpp>
#include <ostream>

namespace beast {

class ostream_test : public beast::unit_test::suite
{
public:
    void run() override
    {
        streambuf sb;
        ostream(sb) << "Hello, world!\n";
        BEAST_EXPECT(boost::lexical_cast<std::string>(
            buffers(sb.data())) == "Hello, world!\n");
    }
};

BEAST_DEFINE_TESTSUITE(ostream,core,beast);

} // beast
