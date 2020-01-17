//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/ostream.hpp>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <ostream>

#include "v1_dynamic_string_buffer.hpp"

namespace boost {
namespace beast {

class ostream_test : public beast::unit_test::suite
{
public:
    void
    testOstream()
    {
        string_view const s = "0123456789abcdef";
        BEAST_EXPECT(s.size() == 16);

        // overflow
        {
            flat_static_buffer<16> b;
            ostream(b) << s;
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        }

        // max_size
        {
            flat_static_buffer<16> b;
            auto os = ostream(b);
            os << s;
            os << '*';
            BEAST_EXPECT(os.bad());
        }

        // max_size (exception
        {
            flat_static_buffer<16> b;
            auto os = ostream(b);
            os.exceptions(os.badbit);
            os << s;
            try
            {
                os << '*';
                fail("missing exception", __FILE__, __LINE__);
            }
            catch(std::ios_base::failure const&)
            {
                pass();
            }
            catch(...)
            {
                fail("wrong exception", __FILE__, __LINE__);
            }
        }
    }

    template<class MakeBuffer>
    void
    testOstreamWithV1orV2(MakeBuffer make_buffer)
    {
        std::string target;
        string_view const s = "0123456789abcdef";
        BEAST_EXPECT(s.size() == 16);

        // overflow
        {
            target.clear();
            ostream(make_buffer(target, 16)) << s;
            BEAST_EXPECT(target == s);
        }

        // max_size
        {
            target.clear();
            flat_static_buffer<16> b;
            auto os = ostream(make_buffer(target, 16));
            os << s;
            os << '*';
            BEAST_EXPECT(os.bad());
            BEAST_EXPECT(target == s);
        }

        // max_size (exception
        {
            target.clear();
            auto os = ostream(make_buffer(target, 16));
            os.exceptions(os.badbit);
            os << s;
            try
            {
                os << '*';
                fail("missing exception", __FILE__, __LINE__);
            }
            catch(std::ios_base::failure const&)
            {
                pass();
            }
            catch(...)
            {
                fail("wrong exception", __FILE__, __LINE__);
            }
        }
    }


    void
    run() override
    {
        testOstreamWithV1orV2([](std::string& s, std::size_t max) { return v1_dynamic_string_buffer(s, max); });
        testOstreamWithV1orV2([](std::string& s, std::size_t max) { return net::dynamic_buffer(s, max); });
        testOstream();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,ostream);

} // beast
} // boost
