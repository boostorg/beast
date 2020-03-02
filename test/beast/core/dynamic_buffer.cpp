//
// Copyright (c) 2016-2020 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core/dynamic_buffer.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/static_buffer.hpp>

namespace boost {
namespace beast {

class dynamic_buffer_test
    : public unit_test::suite
{
    bool buffersEqual(multi_buffer::const_buffers_type a, multi_buffer::const_buffers_type b)
    {
        auto ai = a.begin();
        auto bi = b.begin();

        while (ai != a.end() && bi != b.end())
        {
            auto abuf = *ai++;
            auto bbuf = *bi++;
            if (abuf.data() != bbuf.data())
                return false;
            if (abuf.size() != bbuf.size())
                return false;
        }
        return true;
    }

public:

    template<class DynamicBuffer_v2>
    void implTestV2Conversion(DynamicBuffer_v2 original)
    {
        using decayed_type = typename std::decay<DynamicBuffer_v2>::type;
        BEAST_EXPECT(detail::convertible_to_dynamic_buffer_v2<decayed_type>::value == true);
        BEAST_EXPECT(detail::convertible_to_dynamic_buffer_v2<decayed_type&>::value == true);
        BEAST_EXPECT(detail::convertible_to_dynamic_buffer_v2<decayed_type const&>::value == true);
        BEAST_EXPECT(detail::convertible_to_dynamic_buffer_v2<decayed_type &&>::value == true);

        // taking dynamic_buffer of a DynamicBuffer_v2 results in a cheap
        // copy which refers to the same underlying data
        auto buffer = detail::impl_dynamic_buffer(original);
        BEAST_EXPECTS(typeid(buffer) == typeid(original),
            typeid(buffer).name());

        auto result = buffers_to_string(buffer.data(0, buffer.size()));
        auto expected = buffers_to_string(original.data(0, original.size()));
        BEAST_EXPECTS(result == expected, result);
    }

    void
    testConversion()
    {
        std::string store = "Hello, World!";
        implTestV2Conversion(net::dynamic_buffer(store));

        auto store_vec = std::vector<char>(store.begin(), store.end());
        implTestV2Conversion(net::dynamic_buffer(store_vec));
    }

    void
    testNetV2DynamicBuffers()
    {
        using Elem = char;
        using Traits = std::char_traits<char>;
        using Alloc = std::allocator<char>;

        auto store = std::basic_string<Elem, Traits, Alloc>();
        auto net_dyn_buffer = net::dynamic_buffer(store);
        BEAST_EXPECTS(typeid(net_dyn_buffer) == typeid(net::dynamic_string_buffer<Elem, Traits, Alloc>),
                      typeid(net_dyn_buffer).name());

        auto dd = detail::impl_dynamic_buffer(net_dyn_buffer);
        BEAST_EXPECTS(typeid(dd) == typeid(net::dynamic_string_buffer<Elem, Traits, Alloc>),
                      typeid(dd).name());
    }

    void
    testNetV1DynamicBuffers()
    {
        // @todo Build an archetype or decide not to support legacy buffers
    }

    template<class BufferFactory>
    void
    testByRefV1DynamicBuffers(BufferFactory factory)
    {
        auto storage = factory();
        using storage_type = decltype(storage);

        BEAST_EXPECT(detail::is_dynamic_buffer_v0<storage_type>::value);

        auto dyn_buf = dynamic_buffer(storage);


        BEAST_EXPECT(dyn_buf.size() < dyn_buf.max_size());
        BEAST_EXPECT(dyn_buf.size() == 0);
        BEAST_EXPECT(net::buffer_size(dyn_buf.data(0, dyn_buf.size())) == 0);

        auto do_insert = [&dyn_buf](net::const_buffer source)
        {
            auto start = dyn_buf.size();
            auto len = source.size();
            dyn_buf.grow(len);
            auto insert_region = dyn_buf.data(start, len);
            BEAST_EXPECT(net::buffer_size(insert_region) == len);
            auto copied = net::buffer_copy(insert_region, source);
            BEAST_EXPECT(copied == len);
        };

        do_insert(net::buffer(std::string("0123456789")));
        dyn_buf.shrink(1);
        auto output_region = dyn_buf.data(0, dyn_buf.size());
        BEAST_EXPECT(net::buffer_size(output_region) == 9);
        BEAST_EXPECT(buffers_to_string(output_region) == "012345678");

        do_insert(net::buffer(std::string("9abcdef")));
        dyn_buf.shrink(0);
        output_region = dyn_buf.data(0, dyn_buf.size());
        BEAST_EXPECT(net::buffer_size(output_region) == 16);
        BEAST_EXPECT(buffers_to_string(output_region) == "0123456789abcdef");

        BEAST_THROWS(dyn_buf.grow(1), std::length_error);

        dyn_buf.consume(10);
        output_region = dyn_buf.data(0, dyn_buf.size());
        BEAST_EXPECT(net::buffer_size(output_region) == 6);
        BEAST_EXPECT(buffers_to_string(output_region) == "abcdef");

        dyn_buf.consume(10);
        output_region = dyn_buf.data(0, dyn_buf.size());
        BEAST_EXPECT(net::buffer_size(output_region) == 0);
    }

    void
    run() override
    {
        testConversion();
        testNetV2DynamicBuffers();
        testNetV1DynamicBuffers();
    }

};

BEAST_DEFINE_TESTSUITE(beast,core,dynamic_buffer);

}
}
