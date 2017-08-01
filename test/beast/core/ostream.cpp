//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/ostream.hpp>

#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <ostream>

namespace boost {
namespace beast {

class ostream_test : public beast::unit_test::suite
{
public:
    template<class ConstBufferSequence>
    static
    std::string
    to_string(ConstBufferSequence const& bs)
    {
        using boost::asio::buffer_cast;
        using boost::asio::buffer_size;
        std::string s;
        s.reserve(buffer_size(bs));
        for(boost::asio::const_buffer b : bs)
            s.append(buffer_cast<char const*>(b),
                buffer_size(b));
        return s;
    }

    void
    run() override
    {
        {
            multi_buffer b;
            auto os = ostream(b);
            os << "Hello, world!\n";
            os.flush();
            BEAST_EXPECT(to_string(b.data()) == "Hello, world!\n");
            auto os2 = std::move(os);
        }
        {
            auto const s =
                "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" 
                "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" 
                "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" 
                "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" 
                "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" 
                "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" 
                "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" 
                "0123456789abcdef" "0123456789abcdef" "0123456789abcdef" "0123456789abcdef";
            multi_buffer b;
            ostream(b) << s;
            BEAST_EXPECT(to_string(b.data()) == s);
        }
    }
};

BEAST_DEFINE_TESTSUITE(ostream,core,beast);

} // beast
} // boost
