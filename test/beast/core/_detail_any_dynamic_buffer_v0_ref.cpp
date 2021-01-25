//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/detail/any_dynamic_buffer_v0_ref.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
//#include "test_buffer.hpp"

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/test/test_allocator.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <algorithm>
#include <cctype>

namespace boost {
namespace beast {

class _detail_any_dynamic_buffer_v0_ref_test : public beast::unit_test::suite
{
public:
//    BOOST_STATIC_ASSERT(
//        is_mutable_dynamic_buffer<detail::any_dynamic_buffer_v0_ref>::value);

/*
    void
    testDynamicBuffer()
    {
        flat_buffer b0(30);
        detail::any_dynamic_buffer_v0_ref b(b0);
        BEAST_EXPECT(b.max_size() == 30);
        test_dynamic_buffer(b);
    }
    */

    void
    testSpecialMembers()
    {
        using namespace test;


        // Equal == false
        using a_neq_t = test::test_allocator<char,
            false, true, true, true, true>;

        // construction
        {
            {
                flat_buffer b0;
                detail::any_dynamic_buffer_v0_ref b(b0);
                BEAST_EXPECT(b.capacity() == 0);
            }
            {
                flat_buffer b0{500};
                detail::any_dynamic_buffer_v0_ref b(b0);
                BEAST_EXPECT(b.capacity() == 0);
                BEAST_EXPECT(b.max_size() == 500);
            }
            /*
            {
                a_neq_t a1;
                basic_flat_buffer<a_neq_t> b0{a1};
                any_dynamic_buffer_v0_ref b(b0);
                BEAST_EXPECT(b.get_allocator() == a1);
                a_neq_t a2;
                BEAST_EXPECT(b.get_allocator() != a2);
            }
             */
            {
                a_neq_t a;
                basic_flat_buffer<a_neq_t> b0{500, a};
                detail::any_dynamic_buffer_v0_ref b(b0);
                BEAST_EXPECT(b.capacity() == 0);
                BEAST_EXPECT(b.max_size() == 500);
            }
        }

        // move construction
        {
            /*
            {
                basic_flat_buffer<a_t> b0{30};
                detail::any_dynamic_buffer_v0_ref b1(b0);
                BEAST_EXPECT(b1.get_allocator()->nmove == 0);
                ostream(b1) << "Hello";
                any_dynamic_buffer_v0_ref b2{std::move(b1)};
                BEAST_EXPECT(b2.get_allocator()->nmove == 1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }

            {
                basic_flat_buffer<a_t> b0{30};
                detail::any_dynamic_buffer_v0_ref b1(b0);
                ostream(b1) << "Hello";
                a_t a;
                any_dynamic_buffer_v0_ref b2{std::move(b1), a};
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }
            {
                basic_flat_buffer<a_neq_t> b0{30};
                detail::any_dynamic_buffer_v0_ref b1(b0);
                ostream(b1) << "Hello";
                a_neq_t a;
                any_dynamic_buffer_v0_ref b2{std::move(b1), a};
                BEAST_EXPECT(b1.size() != 0);
                BEAST_EXPECT(b1.capacity() != 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }
             */
        }

        /*
        {
            flat_buffer b0;
            detail::any_dynamic_buffer_v0_ref b1(b0);
            ostream(b1) << "Hello";
            basic_flat_buffer<a_t> b2;
            b2.reserve(1);
            BEAST_EXPECT(b2.capacity() == 1);
            b2 = b1;
            BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            BEAST_EXPECT(b2.capacity() == b2.size());
        }
         */

        // operations
        {
            string_view const s = "Hello, world!";
            flat_buffer b0{64};
            detail::any_dynamic_buffer_v0_ref b1(b0);
            BEAST_EXPECT(b1.size() == 0);
            BEAST_EXPECT(b1.max_size() == 64);
            BEAST_EXPECT(b1.capacity() == 0);
            ostream(b1) << s;
            BEAST_EXPECT(buffers_to_string(b1.data()) == s);
            {
                /*
                flat_buffer b2{b1};
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
                 */
                BEAST_EXPECT(buffers_to_string(b1.data()) == s);
                b1.consume(7);
                BEAST_EXPECT(buffers_to_string(b1.data()) == s.substr(7));

            }
            /*
            {
                flat_buffer b2{32};
                BEAST_EXPECT(b2.max_size() == 32);
                b2 = b1;
                BEAST_EXPECT(b2.max_size() == b1.max_size());
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
             */
        }

        // cause memmove
        {
            flat_buffer b0{20};
            detail::any_dynamic_buffer_v0_ref b(b0);
            ostream(b) << "12345";
            b.consume(3);
            ostream(b) << "67890123";
            BEAST_EXPECT(buffers_to_string(b.data()) == "4567890123");
        }

        // max_size
        {
            flat_buffer b0{10};
            detail::any_dynamic_buffer_v0_ref b(b0);
            BEAST_EXPECT(b.max_size() == 10);
        }

        // allocator max_size
        /*
        {
            basic_flat_buffer<a_t> b;
            auto a = b.get_allocator();
            BOOST_STATIC_ASSERT(
                ! std::is_const<decltype(a)>::value);
            a->max_size = 30;
            try
            {
                b.prepare(1000);
                fail("", __FILE__, __LINE__);
            }
            catch(std::length_error const&)
            {
                pass();
            }
        }
         */

        // read_size
        {
            flat_buffer b0{10};
            detail::any_dynamic_buffer_v0_ref b(b0);
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

        // swap
        /*
        {
            {
                basic_flat_buffer<a_neq_t> b1;
                ostream(b1) << "Hello";
                basic_flat_buffer<a_neq_t> b2;
                BEAST_EXPECT(b1.get_allocator() != b2.get_allocator());
                swap(b1, b2);
                BEAST_EXPECT(b1.get_allocator() != b2.get_allocator());
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                using na_t = test::test_allocator<char,
                    true, true, true, false, true>;
                na_t a1;
                basic_flat_buffer<na_t> b1{a1};
                na_t a2;
                ostream(b1) << "Hello";
                basic_flat_buffer<na_t> b2{a2};
                BEAST_EXPECT(b1.get_allocator() == a1);
                BEAST_EXPECT(b2.get_allocator() == a2);
                swap(b1, b2);
                BEAST_EXPECT(b1.get_allocator() == b2.get_allocator());
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
        }
         */

        // prepare
        {
            flat_buffer b0{100};
            detail::any_dynamic_buffer_v0_ref b(b0);
            b.prepare(10);
            b.commit(10);
            b.prepare(5);
            BEAST_EXPECT(b.capacity() >= 5);
            try
            {
                b.prepare(1000);
                fail("", __FILE__, __LINE__);
            }
            catch(std::length_error const&)
            {
                pass();
            }
        }

    }

    void
    run() override
    {
//        testDynamicBuffer();
        testSpecialMembers();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,_detail_any_dynamic_buffer_v0_ref);

} // beast
} // boost
