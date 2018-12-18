//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/static_buffer.hpp>

#include "buffer_test.hpp"

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <algorithm>
#include <cctype>
#include <string>

namespace boost {
namespace beast {

BOOST_STATIC_ASSERT(
    net::is_dynamic_buffer<static_buffer_base>::value);

class static_buffer_test : public beast::unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        net::is_dynamic_buffer<
            static_buffer_base>::value);
    BOOST_STATIC_ASSERT(
        net::is_const_buffer_sequence<
            static_buffer_base::const_buffers_type>::value);
    BOOST_STATIC_ASSERT(
        net::is_mutable_buffer_sequence<
            static_buffer_base::mutable_data_type>::value);
    BOOST_STATIC_ASSERT(
        net::is_mutable_buffer_sequence<
            static_buffer_base::mutable_buffers_type>::value);
    BOOST_STATIC_ASSERT(std::is_convertible<
        static_buffer_base::mutable_data_type,
        static_buffer_base::const_buffers_type>::value);
#if ! defined( BOOST_LIBSTDCXX_VERSION ) || BOOST_LIBSTDCXX_VERSION >= 50000
# ifndef BOOST_ASIO_ENABLE_BUFFER_DEBUGGING
    BOOST_STATIC_ASSERT(std::is_trivially_copyable<
        static_buffer_base::const_buffers_type>::value);
    BOOST_STATIC_ASSERT(std::is_trivially_copyable<
        static_buffer_base::mutable_data_type>::value);
# endif
#endif

    template<class DynamicBuffer>
    void
    testMutableData()
    {
        DynamicBuffer b;
        DynamicBuffer const& cb = b;
        ostream(b) << "Hello";
        BOOST_STATIC_ASSERT(
            net::is_const_buffer_sequence<
                decltype(cb.data())>::value &&
            ! net::is_mutable_buffer_sequence<
                decltype(cb.data())>::value);
        BOOST_STATIC_ASSERT(
            net::is_const_buffer_sequence<
                decltype(cb.cdata())>::value &&
            ! net::is_mutable_buffer_sequence<
                decltype(cb.cdata())>::value);
        BOOST_STATIC_ASSERT(
            net::is_mutable_buffer_sequence<
                decltype(b.data())>::value);
        std::for_each(
            net::buffers_iterator<decltype(b.data())>::begin(b.data()),
            net::buffers_iterator<decltype(b.data())>::end(b.data()),
            [](char& c)
            {
                c = static_cast<char>(std::toupper(c));
            });
        BEAST_EXPECT(buffers_to_string(b.data()) == "HELLO");
        BEAST_EXPECT(buffers_to_string(b.cdata()) == "HELLO");
    }

    void
    testStaticBuffer()
    {
        using net::buffer;
        using net::buffer_size;
        char buf[12];
        std::string const s = "Hello, world";
        BEAST_EXPECT(s.size() == sizeof(buf));
        for(std::size_t i = 1; i < 4; ++i) {
        for(std::size_t j = 1; j < 4; ++j) {
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        for(std::size_t t = 1; t < 4; ++ t) {
        for(std::size_t u = 1; u < 4; ++ u) {
        std::size_t z = sizeof(buf) - (x + y);
        std::size_t v = sizeof(buf) - (t + u);
        {
            std::memset(buf, 0, sizeof(buf));
            static_buffer<sizeof(buf)> ba;
            {
                auto d = ba.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
            }
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = ba.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
            }
            {
                auto d = ba.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
                ba.commit(buffer_copy(d, buffer(s.data(), x)));
            }
            BEAST_EXPECT(ba.size() == x);
            BEAST_EXPECT(buffer_size(ba.data()) == ba.size());
            {
                auto d = ba.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
            }
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = ba.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
            }
            {
                auto d = ba.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
                ba.commit(buffer_copy(d, buffer(s.data()+x, y)));
            }
            ba.commit(1);
            BEAST_EXPECT(ba.size() == x + y);
            BEAST_EXPECT(buffer_size(ba.data()) == ba.size());
            {
                auto d = ba.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
            }
            {
                auto d = ba.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
            }
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = ba.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
                ba.commit(buffer_copy(d, buffer(s.data()+x+y, z)));
            }
            ba.commit(2);
            BEAST_EXPECT(ba.size() == x + y + z);
            BEAST_EXPECT(buffer_size(ba.data()) == ba.size());
            BEAST_EXPECT(buffers_to_string(ba.data()) == s);
            ba.consume(t);
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            BEAST_EXPECT(buffers_to_string(ba.data()) == s.substr(t, std::string::npos));
            ba.consume(u);
            BEAST_EXPECT(buffers_to_string(ba.data()) == s.substr(t + u, std::string::npos));
            ba.consume(v);
            BEAST_EXPECT(buffers_to_string(ba.data()) == "");
            ba.consume(1);
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            try
            {
                ba.prepare(ba.capacity() - ba.size() + 1);
                fail();
            }
            catch(...)
            {
                pass();
            }
        }
        }}}}}}
    }

    void
    testBuffer()
    {
        string_view const s = "Hello, world!";
        
        // static_buffer_base
        {
            char buf[64];
            static_buffer_base b{buf, sizeof(buf)};
            ostream(b) << s;
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
            b.consume(b.size());
            BEAST_EXPECT(buffers_to_string(b.data()) == "");
        }

        // static_buffer
        {
            static_buffer<64> b1;
            BEAST_EXPECT(b1.size() == 0);
            BEAST_EXPECT(b1.max_size() == 64);
            BEAST_EXPECT(b1.capacity() == 64);
            ostream(b1) << s;
            BEAST_EXPECT(buffers_to_string(b1.data()) == s);
            {
                static_buffer<64> b2{b1};
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
            {
                static_buffer<64> b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
        }

        // cause memmove
        {
            static_buffer<10> b;
            ostream(b) << "12345";
            b.consume(3);
            ostream(b) << "67890123";
            BEAST_EXPECT(buffers_to_string(b.data()) == "4567890123");
            try
            {
                b.prepare(1);
                fail("", __FILE__, __LINE__);
            }
            catch(std::length_error const&)
            {
                pass();
            }
        }

        // read_size
        {
            static_buffer<10> b;
            BEAST_EXPECT(read_size(b, 512) == 10);
            b.prepare(4);
            b.commit(4);
            BEAST_EXPECT(read_size(b, 512) == 6);
            b.consume(2);
            BEAST_EXPECT(read_size(b, 512) == 8);
            b.prepare(8);
            b.commit(8);
            BEAST_EXPECT(read_size(b, 512) == 0);
        }

        // base
        {
            static_buffer<10> b;
            [&](static_buffer_base& base)
            {
                BEAST_EXPECT(base.max_size() == b.capacity());
            }
            (b.base());

            [&](static_buffer_base const& base)
            {
                BEAST_EXPECT(base.max_size() == b.capacity());
            }
            (b.base());
        }
    }

    void run() override
    {
        testBuffer();
        testStaticBuffer();
        testMutableData<static_buffer<32>>();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,static_buffer);

} // beast
} // boost
