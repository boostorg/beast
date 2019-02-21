//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffer_size.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/detail/is_invocable.hpp>

namespace boost {
namespace beast {

namespace {

struct sequence
{
    struct value_type
    {
        operator net::const_buffer() const noexcept
        {
            return {"Hello, world!", 13};
        }
    };

    using const_iterator = value_type const*;

    const_iterator begin() const noexcept
    {
        return &v_;
    }

    const_iterator end() const noexcept
    {
        return begin() + 1;
    }

private:
    value_type v_;
};

struct not_sequence
{
};

} // (anon)

class buffer_size_test : public beast::unit_test::suite
{
public:
    void
    testJavadocs()
    {
        pass();
    }

    void
    testFunction()
    {
        BEAST_EXPECT(buffer_size(
            net::const_buffer("Hello, world!", 13)) == 13);

        BEAST_EXPECT(buffer_size(
            net::mutable_buffer{}) == 0);

        {
            sequence s;
            BEAST_EXPECT(buffer_size(s) == 13);
        }

        {
            std::array<net::const_buffer, 2> s({{
                net::const_buffer("Hello, world!", 13),
                net::const_buffer("Hello, world!", 13)}});
            BEAST_EXPECT(buffer_size(s) == 26);
        }

        BOOST_STATIC_ASSERT(! detail::is_invocable<
            detail::buffer_size_impl,
            std::size_t(not_sequence const&)>::value);
    }

    void run() override
    {
        testFunction();
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffer_size);

} // beast
} // boost
