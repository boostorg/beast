//
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/polymorphic_buffer_sequence.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <numeric>
#include <algorithm>

namespace boost {
namespace beast {
namespace detail {

class polymorphic_buffer_sequence_test : public unit_test::suite
{
    void
    testInvariants()
    {
        BEAST_EXPECT(net::is_const_buffer_sequence<polymorphic_const_buffer_sequence>());
        BEAST_EXPECT(net::is_mutable_buffer_sequence<polymorphic_mutable_buffer_sequence>());
    }

    void testPushFront()
    {
        std::string s1 = "This black cat";
        std::string s2 = " ate a doormat";

        polymorphic_const_buffer_sequence pb{ net::buffer(s2) };
        pb.push_front(net::buffer(s1));
        auto first = pb.begin();
        auto last = pb.end();
        BEAST_EXPECT(std::distance(first, last) == 2);
        BEAST_EXPECTS(boost::beast::buffer_bytes(pb) == 28, std::to_string(boost::beast::buffer_bytes(pb)));
        BEAST_EXPECT(boost::beast::buffers_to_string(pb) == (s1 + s2));


        std::vector<std::string> strings;
        for(int count = 0 ; pb.size() + strings.size() <= pb.static_capacity() ; ++count)
            strings.push_back(std::string("buffer: ") + std::to_string(count));
        std::reverse(strings.begin(), strings.end());
        for(auto& s : strings)
            pb.push_front(net::buffer(s));

        BEAST_EXPECT(pb.size() == pb.static_capacity() + 1);
        auto expected = std::accumulate(strings.rbegin(), strings.rend(), std::string()) + s1 + s2;
        BEAST_EXPECTS(buffers_to_string(pb) == expected,
            buffers_to_string(pb) + " != " + expected);
    }

    void
    testConsume()
    {
        std::string s1 = "This black cat";
        std::string s2 = " ate a doormat";

        polymorphic_const_buffer_sequence pb{ net::buffer(s2) };
        pb.push_front(net::buffer(s1));
        pb.consume(7);
        auto first = pb.begin();
        auto last = pb.end();
        BEAST_EXPECT(std::distance(first, last) == 2);
        BEAST_EXPECTS(boost::beast::buffer_bytes(pb) == 21, std::to_string(boost::beast::buffer_bytes(pb)));
        BEAST_EXPECT(boost::beast::buffers_to_string(pb) == (s1.substr(7) + s2));

        pb.consume(7);
        first = pb.begin();
        last = pb.end();
        BEAST_EXPECT(std::distance(first, last) == 1);
        BEAST_EXPECTS(boost::beast::buffer_bytes(pb) == 14, std::to_string(boost::beast::buffer_bytes(pb)));
        BEAST_EXPECT(boost::beast::buffers_to_string(pb) == s2);
    }

    void
    testPrefix()
    {
        std::vector<std::string> strings(polymorphic_const_buffer_sequence::static_capacity() + 1);
        std::vector<net::const_buffer> buffers(strings.size());
        char c = 'a';
        for (std::size_t i = 0 ; i < strings.size() ; ++i)
        {
            strings[i] = std::string(c, i % 2 == 0 ? 1 : 2);
            ++c;
        }
        std::transform(strings.begin(), strings.end(), buffers.begin(),
                       [](std::string const& x) { return net::buffer(x); });

        auto check = [](polymorphic_const_buffer_sequence const& pb, std::size_t i)
        {
            auto orig = buffers_to_string(pb);
            auto expected = orig.substr(0, std::min(i, orig.size()));
            auto prefix = pb.prefix_copy(i);
            auto str = buffers_to_string(prefix);
            BEAST_EXPECTS(str == expected, std::to_string(pb.size()) + " : " +
                std::to_string(i) + " : " +
                str + " != " + expected);

            // same as buffers_prefix?

            BEAST_EXPECT(str == buffers_to_string(buffers_prefix(i, pb)));
        };

        auto grind = [check](polymorphic_const_buffer_sequence const& pb)
        {
            for (std::size_t i = 0 ; i <= buffer_bytes(pb) + 1 ; ++i)
                check(pb, i);
        };

        auto first = buffers.begin();
        for (auto last = first ; last != buffers.end() ; ++last)
            grind(polymorphic_const_buffer_sequence(first, last));
    }

public:
    void run()
    {
        testInvariants();
        testPushFront();
        testConsume();
        testPrefix();
    }

};

BEAST_DEFINE_TESTSUITE(beast,core,polymorphic_buffer_sequence);

}}}