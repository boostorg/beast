//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_prefix.hpp>

#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/buffer.hpp>
#include <string>

namespace boost {
namespace beast {

class buffers_prefix_test : public beast::unit_test::suite
{
public:
    template<class ConstBufferSequence>
    static
    std::size_t
    bsize1(ConstBufferSequence const& bs)
    {
        std::size_t n = 0;
        for(auto it = bs.begin(); it != bs.end(); ++it)
            n += net::buffer_size(*it);
        return n;
    }

    template<class ConstBufferSequence>
    static
    std::size_t
    bsize2(ConstBufferSequence const& bs)
    {
        std::size_t n = 0;
        for(auto it = bs.begin(); it != bs.end(); it++)
            n += net::buffer_size(*it);
        return n;
    }

    template<class ConstBufferSequence>
    static
    std::size_t
    bsize3(ConstBufferSequence const& bs)
    {
        std::size_t n = 0;
        for(auto it = bs.end(); it != bs.begin();)
            n += net::buffer_size(*--it);
        return n;
    }

    template<class ConstBufferSequence>
    static
    std::size_t
    bsize4(ConstBufferSequence const& bs)
    {
        std::size_t n = 0;
        for(auto it = bs.end(); it != bs.begin();)
        {
            it--;
            n += net::buffer_size(*it);
        }
        return n;
    }

    template<class BufferType>
    void testMatrix()
    {
        using net::buffer_size;
        std::string s = "Hello, world";
        BEAST_EXPECT(s.size() == 12);
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        std::size_t z = s.size() - (x + y);
        {
            std::array<BufferType, 3> bs{{
                BufferType{&s[0], x},
                BufferType{&s[x], y},
                BufferType{&s[x+y], z}}};
            for(std::size_t i = 0; i <= s.size() + 1; ++i)
            {
                auto pb = buffers_prefix(i, bs);
                BEAST_EXPECT(buffers_to_string(pb) == s.substr(0, i));
                auto pb2 = pb;
                BEAST_EXPECT(buffers_to_string(pb2) == buffers_to_string(pb));
                pb = buffers_prefix(0, bs);
                pb2 = pb;
                BEAST_EXPECT(buffer_size(pb2) == 0);
                pb2 = buffers_prefix(i, bs);
                BEAST_EXPECT(buffers_to_string(pb2) == s.substr(0, i));
            }
        }
        }}
    }

    void testEmptyBuffers()
    {
        using net::buffer_size;

        auto pb0 = buffers_prefix(0, net::mutable_buffer{});
        BEAST_EXPECT(buffer_size(pb0) == 0);
        auto pb1 = buffers_prefix(1, net::mutable_buffer{});
        BEAST_EXPECT(buffer_size(pb1) == 0);
        BEAST_EXPECT(net::buffer_copy(pb0, pb1) == 0);

#if 0
        using pb_type = decltype(pb0);
        buffers_suffix<pb_type> cb(pb0);
        BEAST_EXPECT(buffer_size(cb) == 0);
        BEAST_EXPECT(net::buffer_copy(cb, pb1) == 0);
        cb.consume(1);
        BEAST_EXPECT(buffer_size(cb) == 0);
        BEAST_EXPECT(net::buffer_copy(cb, pb1) == 0);

        auto pbc = buffers_prefix(2, cb);
        BEAST_EXPECT(buffer_size(pbc) == 0);
        BEAST_EXPECT(net::buffer_copy(pbc, cb) == 0);
#endif
    }

    void testIterator()
    {
        using net::buffer_size;
        using net::const_buffer;
        char b[3];
        std::array<const_buffer, 3> bs{{
            const_buffer{&b[0], 1},
            const_buffer{&b[1], 1},
            const_buffer{&b[2], 1}}};
        auto pb = buffers_prefix(2, bs);
        BEAST_EXPECT(bsize1(pb) == 2);
        BEAST_EXPECT(bsize2(pb) == 2);
        BEAST_EXPECT(bsize3(pb) == 2);
        BEAST_EXPECT(bsize4(pb) == 2);

        // default ctor is one past the end
        decltype(pb)::const_iterator it;
        BEAST_EXPECT(pb.end() == it);
        BEAST_EXPECT(it == pb.end());
        decltype(pb)::const_iterator it2;
        BEAST_EXPECT(it == it2);
        BEAST_EXPECT(it2 == it);
        it = pb.end();
        it2 = pb.end();
        BEAST_EXPECT(it == it2);
        BEAST_EXPECT(it2 == it);
        decltype(pb)::const_iterator it3(it2);
        BEAST_EXPECT(it3 == it2);
        it = pb.begin();
        BEAST_EXPECT(it != it3);
        it = it3;
        BEAST_EXPECT(it == it3);
    }

    void testInPlaceInit()
    {
        {
            class test_buffers
            {
                net::const_buffer cb_;

            public:
                using const_iterator =
                    net::const_buffer const*;

                explicit
                test_buffers(std::true_type)
                {
                }

                const_iterator
                begin() const
                {
                    return &cb_;
                }

                const_iterator
                end() const
                {
                    return begin() + 1;
                }
            };
            buffers_prefix_view<test_buffers> v(
                2, boost::in_place_init, std::true_type{});
            BEAST_EXPECT(buffer_size(v) == 0);
        }

        {
            char c[2];
            c[0] = 0;
            c[1] = 0;
            buffers_prefix_view<net::const_buffer> v(
                2, boost::in_place_init, c, sizeof(c));
            BEAST_EXPECT(buffer_size(v) == 2);
        }
        {
            char c[2];
            buffers_prefix_view<net::mutable_buffer> v(
                2, boost::in_place_init, c, sizeof(c));
            BEAST_EXPECT(buffer_size(v) == 2);
        }
    }

    void run() override
    {
        testMatrix<net::const_buffer>();
        testMatrix<net::mutable_buffer>();
        testEmptyBuffers();
        testIterator();
        testInPlaceInit();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_prefix);

} // beast
} // boost
