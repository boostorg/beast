//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffer_traits.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <array>

namespace boost {
namespace beast {

class buffer_traits_test : public beast::unit_test::suite
{
public:
    // is_const_buffer_sequence

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer, net::const_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::const_buffer, net::mutable_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_const_buffer_sequence<
        net::mutable_buffer, net::mutable_buffer
            >::value);

    // is_mutable_buffer_sequence

    BOOST_STATIC_ASSERT(is_mutable_buffer_sequence<
            >::value);

    BOOST_STATIC_ASSERT(is_mutable_buffer_sequence<
        net::mutable_buffer
            >::value);

    BOOST_STATIC_ASSERT(is_mutable_buffer_sequence<
        net::mutable_buffer, net::mutable_buffer
            >::value);

    BOOST_STATIC_ASSERT(! is_mutable_buffer_sequence<
        net::const_buffer, net::const_buffer
            >::value);

    BOOST_STATIC_ASSERT(! is_mutable_buffer_sequence<
        net::const_buffer, net::mutable_buffer
            >::value);

    // buffers_type

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            net::const_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            net::const_buffer, net::const_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            net::const_buffer, net::mutable_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer, buffers_type<
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer, buffers_type<
            net::mutable_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer, buffers_type<
            net::mutable_buffer, net::mutable_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            std::array<net::const_buffer, 3>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer, buffers_type<
            std::array<net::mutable_buffer, 3>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer, buffers_type<
            std::array<int, 3>
        >>::value);

    // buffers_iterator_type

    BOOST_STATIC_ASSERT(
        std::is_same<net::const_buffer const*, buffers_iterator_type<
            net::const_buffer
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<net::mutable_buffer const*, buffers_iterator_type<
            net::mutable_buffer
        >>::value);

    // javadoc: buffers_type
    template <class BufferSequence>
    buffers_type <BufferSequence>
    buffers_front (BufferSequence const& buffers)
    {
        static_assert(
            net::is_const_buffer_sequence<BufferSequence>::value,
            "BufferSequence requirements not met");
        auto const first = net::buffer_sequence_begin (buffers);
        if (first == net::buffer_sequence_end (buffers))
            return {};
        return *first;
    }

    void
    testJavadocs()
    {
        // buffers_front
        {
            net::const_buffer cb;
            buffers_front(cb);

            net::mutable_buffer mb;
            buffers_front(mb);
        }

        pass();
    }

    void run() override
    {
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffer_traits);

} // beast
} // boost
