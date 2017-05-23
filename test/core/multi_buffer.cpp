//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/multi_buffer.hpp>

#include "buffer_test.hpp"
#include <beast/core/ostream.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/test/test_allocator.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

namespace beast {

BOOST_STATIC_ASSERT(is_dynamic_buffer<multi_buffer>::value);

class multi_buffer_test : public beast::unit_test::suite
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
    eq(basic_multi_buffer<Alloc1> const& sb1,
        basic_multi_buffer<Alloc2> const& sb2)
    {
        return to_string(sb1.data()) == to_string(sb2.data());
    }

    template<class ConstBufferSequence>
    void
    expect_size(std::size_t n, ConstBufferSequence const& buffers)
    {
        BEAST_EXPECT(test::size_pre(buffers) == n);
        BEAST_EXPECT(test::size_post(buffers) == n);
        BEAST_EXPECT(test::size_rev_pre(buffers) == n);
        BEAST_EXPECT(test::size_rev_post(buffers) == n);
    }

    template<class U, class V>
    static
    void
    self_assign(U& u, V&& v)
    {
        u = std::forward<V>(v);
    }

    void testSpecialMembers()
    {
        using boost::asio::buffer;
        std::string const s = "Hello, world";
        BEAST_EXPECT(s.size() == 12);
        for(std::size_t i = 1; i < 12; ++i) {
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        std::size_t z = s.size() - (x + y);
        {
            multi_buffer b(i);
            b.commit(buffer_copy(b.prepare(x), buffer(s.data(), x)));
            b.commit(buffer_copy(b.prepare(y), buffer(s.data()+x, y)));
            b.commit(buffer_copy(b.prepare(z), buffer(s.data()+x+y, z)));
            BEAST_EXPECT(to_string(b.data()) == s);
            {
                multi_buffer sb2(b);
                BEAST_EXPECT(eq(b, sb2));
            }
            {
                multi_buffer sb2;
                sb2 = b;
                BEAST_EXPECT(eq(b, sb2));
            }
            {
                multi_buffer sb2(std::move(b));
                BEAST_EXPECT(to_string(sb2.data()) == s);
                expect_size(0, b.data());
                b = std::move(sb2);
                BEAST_EXPECT(to_string(b.data()) == s);
                expect_size(0, sb2.data());
            }
            self_assign(b, b);
            BEAST_EXPECT(to_string(b.data()) == s);
            self_assign(b, std::move(b));
            BEAST_EXPECT(to_string(b.data()) == s);
        }
        }}}
        try
        {
            multi_buffer sb0(0);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
    }

    void
    testAllocator()
    {
        using test::test_allocator;
        // VFALCO This needs work
        {
            using alloc_type =
                test_allocator<char, false, false, false, false, false>;
            using type = basic_multi_buffer<alloc_type>;
            type b;
        }
        {
            using alloc_type =
                test_allocator<char, false, false, false, false, false>;
            using type = basic_multi_buffer<alloc_type>;
            type b;
            type b2(b);
            type b3(b, alloc_type{});
        }
    }

    void
    testPrepare()
    {
        using boost::asio::buffer_size;
        {
            multi_buffer b(2);
            BEAST_EXPECT(buffer_size(b.prepare(5)) == 5);
            BEAST_EXPECT(buffer_size(b.prepare(8)) == 8);
            BEAST_EXPECT(buffer_size(b.prepare(7)) == 7);
        }
        {
            multi_buffer b(2);
            b.prepare(2);
            BEAST_EXPECT(test::buffer_count(b.prepare(5)) == 2);
            BEAST_EXPECT(test::buffer_count(b.prepare(8)) == 3);
            BEAST_EXPECT(test::buffer_count(b.prepare(4)) == 2);
        }
    }

    void testCommit()
    {
        multi_buffer b(2);
        b.prepare(2);
        b.prepare(5);
        b.commit(1);
        expect_size(1, b.data());
    }

    void testConsume()
    {
        multi_buffer b(1);
        expect_size(5, b.prepare(5));
        b.commit(3);
        expect_size(3, b.data());
        b.consume(1);
        expect_size(2, b.data());
    }

    void testMatrix()
    {
        using boost::asio::buffer;
        using boost::asio::buffer_size;
        std::string const s = "Hello, world";
        BEAST_EXPECT(s.size() == 12);
        for(std::size_t i = 1; i < 12; ++i) {
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        for(std::size_t t = 1; t < 4; ++ t) {
        for(std::size_t u = 1; u < 4; ++ u) {
        std::size_t z = s.size() - (x + y);
        std::size_t v = s.size() - (t + u);
        {
            multi_buffer b(i);
            {
                auto d = b.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
            }
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = b.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
            }
            {
                auto d = b.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
                b.commit(buffer_copy(d, buffer(s.data(), x)));
            }
            BEAST_EXPECT(b.size() == x);
            BEAST_EXPECT(buffer_size(b.data()) == b.size());
            {
                auto d = b.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
            }
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = b.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
            }
            {
                auto d = b.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
                b.commit(buffer_copy(d, buffer(s.data()+x, y)));
            }
            b.commit(1);
            BEAST_EXPECT(b.size() == x + y);
            BEAST_EXPECT(buffer_size(b.data()) == b.size());
            {
                auto d = b.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
            }
            {
                auto d = b.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
            }
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = b.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
                b.commit(buffer_copy(d, buffer(s.data()+x+y, z)));
            }
            b.commit(2);
            BEAST_EXPECT(b.size() == x + y + z);
            BEAST_EXPECT(buffer_size(b.data()) == b.size());
            BEAST_EXPECT(to_string(b.data()) == s);
            b.consume(t);
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            BEAST_EXPECT(to_string(b.data()) == s.substr(t, std::string::npos));
            b.consume(u);
            BEAST_EXPECT(to_string(b.data()) == s.substr(t + u, std::string::npos));
            b.consume(v);
            BEAST_EXPECT(to_string(b.data()) == "");
            b.consume(1);
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
        }
        }}}}}
    }

    void testIterators()
    {
        using boost::asio::buffer_size;
        multi_buffer b(1);
        b.prepare(1);
        b.commit(1);
        b.prepare(2);
        b.commit(2);
        expect_size(3, b.data());
        b.prepare(1);
        expect_size(3, b.prepare(3));
        b.commit(2);
        BEAST_EXPECT(test::buffer_count(b.data()) == 4);
    }

    void testCapacity()
    {
        using beast::detail::read_size_helper;
        using boost::asio::buffer_size;
        {
            multi_buffer b{10};
            BEAST_EXPECT(b.alloc_size() == 10);
            BEAST_EXPECT(read_size_helper(b, 1) == 1);
            BEAST_EXPECT(read_size_helper(b, 10) == 10);
            BEAST_EXPECT(read_size_helper(b, 20) == 10);
            BEAST_EXPECT(read_size_helper(b, 1000) == 10);
            b.prepare(3);
            b.commit(3);
            BEAST_EXPECT(read_size_helper(b, 10) == 7);
            BEAST_EXPECT(read_size_helper(b, 1000) == 7);
        }
        {
            multi_buffer b(1000);
            BEAST_EXPECT(b.alloc_size() == 1000);
            BEAST_EXPECT(read_size_helper(b, 1) == 1);
            BEAST_EXPECT(read_size_helper(b, 1000) == 1000);
            BEAST_EXPECT(read_size_helper(b, 2000) == 1000);
            b.prepare(3);
            BEAST_EXPECT(read_size_helper(b, 1) == 1);
            BEAST_EXPECT(read_size_helper(b, 1000) == 1000);
            BEAST_EXPECT(read_size_helper(b, 2000) == 1000);
            b.commit(3);
            BEAST_EXPECT(read_size_helper(b, 1) == 1);
            BEAST_EXPECT(read_size_helper(b, 1000) == 997);
            BEAST_EXPECT(read_size_helper(b, 2000) == 997);
            b.consume(2);
            BEAST_EXPECT(read_size_helper(b, 1) == 1);
            BEAST_EXPECT(read_size_helper(b, 1000) == 997);
            BEAST_EXPECT(read_size_helper(b, 2000) == 997);
        }
        {
            multi_buffer b{2};
            BEAST_EXPECT(b.alloc_size() == 2);
            BEAST_EXPECT(test::buffer_count(b.prepare(2)) == 1);
            BEAST_EXPECT(test::buffer_count(b.prepare(3)) == 2);
            BEAST_EXPECT(buffer_size(b.prepare(5)) == 5);
            BEAST_EXPECT(read_size_helper(b, 10) == 6);
        }
        {
            auto avail =
                [](multi_buffer const& b)
                {
                    return b.capacity() - b.size();
                };
            multi_buffer b{100};
            BEAST_EXPECT(b.alloc_size() == 100);
            BEAST_EXPECT(avail(b) == 0);
            b.prepare(100);
            BEAST_EXPECT(avail(b) == 100);
            b.commit(100);
            BEAST_EXPECT(avail(b) == 0);
            b.consume(100);
            BEAST_EXPECT(avail(b) == 0);
            b.alloc_size(200);
            BEAST_EXPECT(b.alloc_size() == 200);
            b.prepare(1);
            BEAST_EXPECT(avail(b) == 200);
        }
    }

    void run() override
    {
        test::check_read_size_helper<multi_buffer>();

        testSpecialMembers();
        testAllocator();
        testPrepare();
        testCommit();
        testConsume();
        testMatrix();
        testIterators();
        testCapacity();
    }
};

BEAST_DEFINE_TESTSUITE(multi_buffer,core,beast);

} // beast
