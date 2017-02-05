//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/flat_streambuf.hpp>

#include "buffer_test.hpp"
#include <beast/core/to_string.hpp>
#include <beast/unit_test/suite.hpp>

namespace beast {

static_assert(is_DynamicBuffer<flat_streambuf>::value,
    "DynamicBuffer requirements not met");

class flat_streambuf_test : public beast::unit_test::suite
{
public:
    template<class Alloc1, class Alloc2>
    static
    bool
    eq(basic_flat_streambuf<Alloc1> const& sb1,
        basic_flat_streambuf<Alloc2> const& sb2)
    {
        return to_string(sb1.data()) == to_string(sb2.data());
    }

    void
    testSpecialMembers()
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        {
            flat_streambuf fb{1};
            BEAST_EXPECT(fb.max_size() == 1);
        }
        {
            flat_streambuf fb{1024};
            BEAST_EXPECT(fb.max_size() == 1024);
        }
        std::string const s = "Hello, world!";
        for(std::size_t i = 1; i < s.size() - 1; ++i)
        {
            flat_streambuf fb;
            fb.commit(buffer_copy(
                fb.prepare(i), buffer(s)));
            fb.commit(buffer_copy(
                fb.prepare(s.size() - i),
                    buffer(s.data() + i, s.size() - i)));
            BEAST_EXPECT(to_string(fb.data()) == s);
            {
                flat_streambuf fb2{fb};
                BEAST_EXPECT(eq(fb2, fb));
                flat_streambuf fb3{std::move(fb2)};
                BEAST_EXPECT(eq(fb3, fb));
                BEAST_EXPECT(! eq(fb2, fb3));
                BEAST_EXPECT(fb2.size() == 0);
            }

            using alloc_type = std::allocator<double>;
            using streambuf_type =
                basic_flat_streambuf<alloc_type>;
            alloc_type alloc;
            {
                streambuf_type fba{alloc, 1};
                BEAST_EXPECT(fba.max_size() == 1);
            }
            {
                streambuf_type fba{alloc, 1024};
                BEAST_EXPECT(fba.max_size() == 1024);
            }
            {
                streambuf_type fb2{fb};
                BEAST_EXPECT(eq(fb2, fb));
                streambuf_type fb3{std::move(fb2)};
                BEAST_EXPECT(eq(fb3, fb));
                BEAST_EXPECT(! eq(fb2, fb3));
                BEAST_EXPECT(fb2.size() == 0);
            }
            {
                streambuf_type fb2{fb, alloc};
                BEAST_EXPECT(eq(fb2, fb));
                streambuf_type fb3{std::move(fb2), alloc};
                BEAST_EXPECT(eq(fb3, fb));
                BEAST_EXPECT(! eq(fb2, fb3));
                BEAST_EXPECT(fb2.size() == 0);
            }
        }
    }

    void
    testStream()
    {
        using boost::asio::buffer_size;

        flat_streambuf fb;
        BEAST_EXPECT(fb.size() == 0);
        BEAST_EXPECT(fb.capacity() == 0);

        BEAST_EXPECT(buffer_size(fb.prepare(100)) == 100);
        BEAST_EXPECT(fb.size() == 0);
        BEAST_EXPECT(fb.capacity() == 100);

        fb.commit(20);
        BEAST_EXPECT(fb.size() == 20);
        BEAST_EXPECT(fb.capacity() == 100);

        fb.consume(5);
        BEAST_EXPECT(fb.size() == 15);
        BEAST_EXPECT(fb.capacity() == 95);

        fb.prepare(80);
        fb.commit(80);
        BEAST_EXPECT(fb.size() == 95);
        BEAST_EXPECT(fb.capacity() == 100);

        fb.shrink_to_fit();
        BEAST_EXPECT(fb.size() == 95);
        BEAST_EXPECT(fb.capacity() == 95);
    }

    void
    testPrepare()
    {
        flat_streambuf fb;
        fb.prepare(20);
        BEAST_EXPECT(fb.capacity() == 20);
        fb.commit(10);
        BEAST_EXPECT(fb.capacity() == 20);
        fb.consume(4);
        BEAST_EXPECT(fb.capacity() == 16);
        fb.prepare(14);
        BEAST_EXPECT(fb.size() == 6);
        BEAST_EXPECT(fb.capacity() == 20);
        fb.consume(10);
        BEAST_EXPECT(fb.size() == 0);
        BEAST_EXPECT(fb.capacity() == 20);
    }

    void
    testMax()
    {
        flat_streambuf fb{1};
        try
        {
            fb.prepare(2);
            fail("", __FILE__, __LINE__);
        }
        catch(std::length_error const&)
        {
            pass();
        }
    }

    void
    run() override
    {
        testSpecialMembers();
        testStream();
        testPrepare();
        testMax();
    }
};

BEAST_DEFINE_TESTSUITE(flat_streambuf,core,beast);

} // beast
