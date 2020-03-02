//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_adaptor.hpp>

#include "test_buffer.hpp"

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/streambuf.hpp>
#include <iterator>

namespace boost {
namespace beast {

class buffers_adaptor_test : public unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        is_mutable_dynamic_buffer<
            buffers_adaptor<buffers_triple>>::value);

    void
    testDynamicBuffer()
    {
        char s[13];
        buffers_triple tb(s, sizeof(s));
        buffers_adaptor<buffers_triple> b(tb);
        test_dynamic_buffer(b);
    }

    void
    testSpecial()
    {
        char s1[13];
        buffers_triple tb1(s1, sizeof(s1));
        BEAST_EXPECT(buffer_bytes(tb1) == sizeof(s1));

        char s2[15];
        buffers_triple tb2(s2, sizeof(s2));
        BEAST_EXPECT(buffer_bytes(tb2) == sizeof(s2));

        {
            // construction

            buffers_adaptor<buffers_triple> b1(tb1);
            BEAST_EXPECT(b1.value() == tb1);

            buffers_adaptor<buffers_triple> b2(tb2);
            BEAST_EXPECT(b2.value() == tb2);

            buffers_adaptor<buffers_triple> b3(b2);
            BEAST_EXPECT(b3.value() == tb2);

            char s3[15];
            buffers_adaptor<buffers_triple> b4(
                boost::in_place_init, s3, sizeof(s3));
            BEAST_EXPECT(b4.value() == buffers_triple(s3, sizeof(s3)));

            // assignment

            b3 = b1;
            BEAST_EXPECT(b3.value() == tb1);
        }
    }

    void
    testIssue386()
    {
        using type = net::streambuf;
        type buffer;
        buffers_adaptor<
            type::mutable_buffers_type> ba{buffer.prepare(512)};
        read_size(ba, 1024);
    }

    struct pad
    {
        pad() = default;
        pad(pad const&) = delete;
        pad& operator=(pad const&) = delete;


        template<class...Sizes>
        void
        allocate(Sizes... size_values)
        {
            auto sizes = std::array<std::size_t, sizeof...(size_values)>{
                std::size_t(size_values)...
            };

            blocks.resize(sizes.size());
            buffers.resize(sizes.size());
            for (std::size_t i = 0 ; i < sizes.size() ; ++i)
            {
                blocks[i] = std::string(sizes[i], ' ');
                buffers[i] = net::buffer(blocks[i]);
            }
        }

        auto
        create()
        -> buffers_adaptor<std::vector<net::mutable_buffer>>
        {
            return buffers_adaptor<std::vector<net::mutable_buffer>>(buffers);
        }


        template<class...Sizes>
        auto
        generate(Sizes... size_values)
        -> buffers_adaptor<std::vector<net::mutable_buffer>>
        {
            allocate(size_values...);
            return create();
        }

        std::vector<std::string> blocks;
        std::vector<net::mutable_buffer> buffers;
    };

    void
    testV2Interop()
    {
        pad my_pad;

        test_dynamic_buffer_v0_v2_consistency(
            [&]{ return my_pad.generate(4096, 2048, 2048); });
        test_dynamic_buffer_v0_v2_operation(my_pad.generate(16));

        my_pad.allocate(1000000,1000000,1000000);
        struct generator
        {
            pad& my_pad;
            static constexpr std::size_t size() { return 26; }
            buffers_adaptor<std::vector<net::mutable_buffer>>
            make_store()
            {
                return my_pad.create();
            }
        };
        test_v0_v2_data_rotations(generator{my_pad});
    }

    void
    run() override
    {
        testDynamicBuffer();
        testSpecial();
        testIssue386();
        testV2Interop();
#if 0
        testBuffersAdapter();
        testCommit();
#endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_adaptor);

} // beast
} // boost
