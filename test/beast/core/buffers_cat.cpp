//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_cat.hpp>

#include "buffer_test.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/streambuf.hpp>
#include <iterator>
#include <list>
#include <type_traits>
#include <vector>

namespace boost {
namespace beast {

class buffers_cat_test : public unit_test::suite
{
public:
    template<class ConstBufferSequence>
    static
    std::size_t
    buffers_length(
        ConstBufferSequence const& buffers)
    {
        return std::distance(
            net::buffer_sequence_begin(buffers),
            net::buffer_sequence_end(buffers));
    }

    void
    testDefaultIterators()
    {
        // default ctor is one past the end
        char c[2];
        auto bs = buffers_cat(
            net::const_buffer(&c[0], 1),
            net::const_buffer(&c[1], 1));
        decltype(bs)::const_iterator it;
        BEAST_EXPECT(bs.end() == it);
        BEAST_EXPECT(it == bs.end());
        decltype(bs)::const_iterator it2;
        BEAST_EXPECT(it == it2);
        BEAST_EXPECT(it2 == it);
        it = bs.end();
        it2 = bs.end();
        BEAST_EXPECT(it == it2);
        BEAST_EXPECT(it2 == it);
        decltype(bs)::const_iterator it3(it2);
        BEAST_EXPECT(it3 == it2);
        it = bs.begin();
        BEAST_EXPECT(it != it3);
        it = it3;
        BEAST_EXPECT(it == it3);

        // dereferencing default iterator should throw
        try
        {
            it = {};
            *it;
            fail();
        }
        catch(std::logic_error const&)
        {
            pass();
        }
        catch(...)
        {
            fail();
        }
    }

    void
    testBufferSequence()
    {
        string_view s = "Hello, world!";
        net::const_buffer b1(s.data(), 6);
        net::const_buffer b2(
            s.data() + b1.size(), s.size() - b1.size());
        test_buffer_sequence(
            *this, buffers_cat(b1, b2));
    }

    template<class F>
    void
    checkException(F&& f)
    {
        try
        {
            f();
            fail("missing exception", __FILE__, __LINE__);
        }
        catch(std::logic_error const&)
        {
            pass();
        }
        catch(...)
        {
            fail("wrong exception", __FILE__, __LINE__);
        }
    }

    void
    testExceptions()
    {
        net::const_buffer b1{"He", 2};
        net::const_buffer b2{"llo,", 4};
        net::const_buffer b3{" world!", 7};

        auto const b = beast::buffers_cat(b1, b2, b3);

        // Dereferencing a default-constructed iterator
        checkException(
            []
            {
                *(decltype(b)::const_iterator{});
            });

        // Incrementing a default-constructed iterator
        checkException(
            []
            {
                ++(decltype(b)::const_iterator{});
            });

        // Decrementing a default-constructed iterator
        checkException(
            []
            {
                --(decltype(b)::const_iterator{});
            });

        // Decrementing an iterator to the beginning
        checkException(
            [&b]
            {
                --b.begin();
            });

        // Dereferencing an iterator to the end
        checkException(
            [&b]
            {
                *b.end();
            });

        // Incrementing an iterator to the end
        checkException(
            [&b]
            {
                ++b.end();
            });
    }

    void
    testEmpty()
    {
        using net::buffer_size;
 
        struct empty_sequence
        {
            using value_type = net::const_buffer;
            using const_iterator = value_type const*;

            const_iterator
            begin() const noexcept
            {
                return &v_;
            }

            const_iterator
            end() const noexcept
            {
                return begin();
            }

        private:
            value_type v_;
        };

        {
            net::const_buffer b0{};
            net::const_buffer b1{"He", 2};
            net::const_buffer b2{"llo,", 4};
            net::const_buffer b3{" world!", 7};

            {
                auto const b = beast::buffers_cat(b0, b0);
                BEAST_EXPECT(buffer_size(b) == 0);
                BEAST_EXPECT(buffers_length(b) == 0);
            }
            {
                auto const b = beast::buffers_cat(b0, b0, b0, b0);
                BEAST_EXPECT(buffer_size(b) == 0);
                BEAST_EXPECT(buffers_length(b) == 0);
            }
            {
                auto const b = beast::buffers_cat(b1, b2, b3);
                BEAST_EXPECT(beast::buffers_to_string(b) == "Hello, world!");
                BEAST_EXPECT(buffers_length(b) == 3);
                test_buffer_sequence(*this, b);
            }
            {
                auto const b = beast::buffers_cat(b0, b1, b2, b3);
                BEAST_EXPECT(beast::buffers_to_string(b) == "Hello, world!");
                BEAST_EXPECT(buffers_length(b) == 3);
                test_buffer_sequence(*this, b);
            }
            {
                auto const b = beast::buffers_cat(b1, b0, b2, b3);
                BEAST_EXPECT(beast::buffers_to_string(b) == "Hello, world!");
                BEAST_EXPECT(buffers_length(b) == 3);
                test_buffer_sequence(*this, b);
            }
            {
                auto const b = beast::buffers_cat(b1, b2, b0, b3);
                BEAST_EXPECT(beast::buffers_to_string(b) == "Hello, world!");
                BEAST_EXPECT(buffers_length(b) == 3);
                test_buffer_sequence(*this, b);
            }
            {
                auto const b = beast::buffers_cat(b1, b2, b3, b0);
                BEAST_EXPECT(beast::buffers_to_string(b) == "Hello, world!");
                BEAST_EXPECT(buffers_length(b) == 3);
                test_buffer_sequence(*this, b);
            }
        }

        {
            auto e1 = net::const_buffer{};
            auto b1 = std::array<net::const_buffer, 3>{{
                e1,
                net::const_buffer{"He", 2},
                net::const_buffer{"l", 1} }};
            auto b2 = std::array<net::const_buffer, 3>{{
                net::const_buffer{"lo", 2},
                e1,
                net::const_buffer{", ", 2} }};
            auto b3 = std::array<net::const_buffer, 3>{{
                net::const_buffer{"w", 1},
                net::const_buffer{"orld!", 5},
                e1 }};
            {
                auto const b = beast::buffers_cat(
                    e1, b1, e1, b2, e1, b3, e1);
                BEAST_EXPECT(beast::buffers_to_string(b) == "Hello, world!");
                BEAST_EXPECT(buffers_length(b) == 6);
            }
        }

        {
            auto e1 = net::const_buffer{};
            auto e2 = empty_sequence{};
            auto b1 = std::array<net::const_buffer, 3>{{
                e1,
                net::const_buffer{"He", 2},
                net::const_buffer{"l", 1} }};
            auto b2 = std::array<net::const_buffer, 3>{{
                net::const_buffer{"lo", 2},
                e1,
                net::const_buffer{", ", 2} }};
            auto b3 = std::array<net::const_buffer, 3>{
                net::const_buffer{"w", 1},
                net::const_buffer{"orld!", 5},
                e1
            };
            {
                auto const b = beast::buffers_cat(
                    e2, b1, e2, b2, e2, b3, e2);
                BEAST_EXPECT(beast::buffers_to_string(b) == "Hello, world!");
                BEAST_EXPECT(buffers_length(b) == 6);
            }
        }
    }

    void
    testGccWarning1()
    {
        using boost::beast::buffers_cat;
        using boost::beast::buffers_suffix;
        using net::buffer;
        using net::const_buffer;

        char out[64];
        std::array<const_buffer, 2> buffers{
            {buffer("Hello, "), buffer("world!")}};
        std::size_t i = 3;
        buffers_suffix<std::array<const_buffer, 2>> cb(buffers);
        cb.consume(i);
        net::buffer_copy(
            buffer(out),
            buffers_cat(cb, cb));
    }

    // VFALCO Some version of g++ incorrectly complain about
    //        uninitialized values when buffers_cat and
    //        buffers_prefix are used together.
    void
    testGccWarning2()
    {
        using net::buffer;
        using net::buffer_copy;
        using net::const_buffer;

        char out[64];
        const_buffer buffers("Hello, world!", 13);
        std::size_t i = 3;
        buffers_suffix<net::const_buffer> cb{buffers};
        cb.consume(i);
        net::buffer_copy(
            net::buffer(out),
            buffers_cat(buffers_prefix(i, buffers), cb));
    }

    void run() override
    {
        testBufferSequence();
        testExceptions();
        testEmpty();
        testGccWarning1();
        testGccWarning2();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_cat);

} // beast
} // boost
