//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/flat_streambuf.hpp>

#include "buffer_test.hpp"
#include <beast/core/ostream.hpp>
#include <beast/test/test_allocator.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/lexical_cast.hpp>
#include <algorithm>

namespace beast {

static_assert(is_DynamicBuffer<flat_streambuf>::value,
    "DynamicBuffer requirements not met");

class flat_streambuf_test : public beast::unit_test::suite
{
public:
    template<class ConstBufferSequence>
    static
    std::string
    to_string(ConstBufferSequence const& bs)
    {
        return boost::lexical_cast<
            std::string>(buffers(bs));
    }

    template<class Alloc1, class Alloc2>
    static
    bool
    eq(basic_flat_streambuf<Alloc1> const& sb1,
        basic_flat_streambuf<Alloc2> const& sb2)
    {
        return to_string(sb1.data()) == to_string(sb2.data());
    }

    template<
        bool Equal, bool Assign, bool Move, bool Swap, bool Select>
    void
    testCtor()
    {
        using allocator = test::test_allocator<char,
            Equal, Assign, Move, Swap, Select>;
        {
            using boost::asio::buffer_size;
            basic_flat_streambuf<allocator> b1{10};
            BEAST_EXPECT(b1.size() == 0);
            BEAST_EXPECT(b1.capacity() == 0);
            BEAST_EXPECT(b1.max_size() == 10);
            b1.prepare(1);
            b1.commit(1);
            basic_flat_streambuf<allocator> b2{std::move(b1)};
            BEAST_EXPECT(b1.capacity() == 0);
            BEAST_EXPECT(b1.max_size() == 10);
            BEAST_EXPECT(b2.size() == 1);
            BEAST_EXPECT(b2.max_size() == 10);
            BEAST_EXPECT(buffer_size(b1.data()) == 0);
            BEAST_EXPECT(buffer_size(b1.prepare(1)) == 1);
        }
        {
            basic_flat_streambuf<allocator> b1{10};
            basic_flat_streambuf<allocator> b2{std::move(b1), allocator{}};
        }
        {
            basic_flat_streambuf<allocator> b1{10};
            basic_flat_streambuf<allocator> b2{b1};
        }
        {
            basic_flat_streambuf<allocator> b1{10};
            basic_flat_streambuf<allocator> b2{b1, allocator{}};
        }
        {
            flat_streambuf b1{10};
            b1.prepare(1);
            b1.commit(1);
            basic_flat_streambuf<allocator> b2{b1};
            BEAST_EXPECT(b2.size() == 1);
        }
        {
            basic_flat_streambuf<allocator> b1{10};
        }
        {
            basic_flat_streambuf<allocator> b1{allocator{}, 10};
        }
    }

    void
    testCtors()
    {
        testCtor<false, false, false, false, false>();
        testCtor<false, false, false, false,  true>();
        testCtor<false, false, false,  true, false>();
        testCtor<false, false, false,  true,  true>();
        testCtor<false, false,  true, false, false>();
        testCtor<false, false,  true, false,  true>();
        testCtor<false, false,  true,  true, false>();
        testCtor<false, false,  true,  true,  true>();
        testCtor<false,  true, false, false, false>();
        testCtor<false,  true, false, false,  true>();
        testCtor<false,  true, false,  true, false>();
        testCtor<false,  true, false,  true,  true>();
        testCtor<false,  true,  true, false, false>();
        testCtor<false,  true,  true, false,  true>();
        testCtor<false,  true,  true,  true, false>();
        testCtor<false,  true,  true,  true,  true>();
        testCtor< true, false, false, false, false>();
        testCtor< true, false, false, false,  true>();
        testCtor< true, false, false,  true, false>();
        testCtor< true, false, false,  true,  true>();
        testCtor< true, false,  true, false, false>();
        testCtor< true, false,  true, false,  true>();
        testCtor< true, false,  true,  true, false>();
        testCtor< true, false,  true,  true,  true>();
        testCtor< true,  true, false, false, false>();
        testCtor< true,  true, false, false,  true>();
        testCtor< true,  true, false,  true, false>();
        testCtor< true,  true, false,  true,  true>();
        testCtor< true,  true,  true, false, false>();
        testCtor< true,  true,  true, false,  true>();
        testCtor< true,  true,  true,  true, false>();
        testCtor< true,  true,  true,  true,  true>();
    }

    void
    testOperations()
    {
        //
        // reserve
        //

        {
            flat_streambuf b{10};
            b.prepare(1);
            b.commit(1);
            b.reserve(2);
            BEAST_EXPECT(b.size() == 1);
        }
        {
            flat_streambuf b{10};
            try
            {
                b.reserve(11);
                fail("", __FILE__, __LINE__);
            }
            catch(std::length_error const&)
            {
                pass();
            }
        }
    }

    void
    testSpecialMembers()
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        {
            flat_streambuf fb{10};
            BEAST_EXPECT(fb.max_size() == 10);
        }
        {
            flat_streambuf fb{1024};
            BEAST_EXPECT(fb.max_size() == 1024);
        }
        std::string const s = "Hello, world!";
        for(std::size_t i = 1; i < s.size() - 1; ++i)
        {
            flat_streambuf fb{1024};
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
            using type =
                basic_flat_streambuf<alloc_type>;
            alloc_type alloc;
            {
                type fba{alloc, 1};
                BEAST_EXPECT(fba.max_size() == 1);
            }
            {
                type fba{alloc, 1024};
                BEAST_EXPECT(fba.max_size() == 1024);
            }
            {
                type fb2{fb};
                BEAST_EXPECT(eq(fb2, fb));
                type fb3{std::move(fb2)};
                BEAST_EXPECT(eq(fb3, fb));
                BEAST_EXPECT(! eq(fb2, fb3));
                BEAST_EXPECT(fb2.size() == 0);
            }
            {
                type fb2{fb, alloc};
                BEAST_EXPECT(eq(fb2, fb));
                type fb3{std::move(fb2), alloc};
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

        flat_streambuf fb{100};
        BEAST_EXPECT(fb.size() == 0);
        BEAST_EXPECT(fb.capacity() == 0);

        BEAST_EXPECT(buffer_size(fb.prepare(100)) == 100);
        BEAST_EXPECT(fb.size() == 0);
        BEAST_EXPECT(fb.capacity() > 0);

        fb.commit(20);
        BEAST_EXPECT(fb.size() == 20);
        BEAST_EXPECT(fb.capacity() == 100);

        fb.consume(5);
        BEAST_EXPECT(fb.size() == 15);
        BEAST_EXPECT(fb.capacity() == 100);

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
        flat_streambuf fb{100};
        fb.prepare(20);
        BEAST_EXPECT(fb.capacity() == 100);
        fb.commit(10);
        BEAST_EXPECT(fb.capacity() == 100);
        fb.consume(4);
        BEAST_EXPECT(fb.capacity() == 100);
        fb.prepare(14);
        BEAST_EXPECT(fb.size() == 6);
        BEAST_EXPECT(fb.capacity() == 100);
        fb.consume(10);
        BEAST_EXPECT(fb.size() == 0);
        BEAST_EXPECT(fb.capacity() == 100);
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
        testCtors();
        testOperations();

        testSpecialMembers();
        testStream();
        testPrepare();
        testMax();
    }
};

BEAST_DEFINE_TESTSUITE(flat_streambuf,core,beast);

} // beast
