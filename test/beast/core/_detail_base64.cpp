//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/base64.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <string>

namespace boost {
namespace beast {
namespace detail {

class base64_test : public beast::unit_test::suite
{
public:
    std::string
    base64_encode(
        std::uint8_t const* data,
        std::size_t len)
    {
        std::string dest;
        dest.resize(base64::encoded_size(len));
        dest.resize(base64::encode(&dest[0], data, len));
        return dest;
    }

    std::string
    base64_encode(core::string_view s)
    {
        return base64_encode(reinterpret_cast <
            std::uint8_t const*> (s.data()), s.size());
    }

    std::string
    base64_decode(core::string_view data)
    {
        std::string dest;
        dest.resize(base64::decoded_size(data.size()));
        auto const result = base64::decode(
            &dest[0], data.data(), data.size());
        dest.resize(result.first);
        return dest;
    }

    void
    check(std::string const& in, std::string const& out)
    {
        auto const encoded = base64_encode(in);
        BEAST_EXPECT(encoded == out);
        BEAST_EXPECT(base64_decode(encoded) == in);
    }

    void
    testRoundTrip()
    {
        check("",       "");
        check("f",      "Zg==");
        check("fo",     "Zm8=");
        check("foo",    "Zm9v");
        check("foob",   "Zm9vYg==");
        check("fooba",  "Zm9vYmE=");
        check("foobar", "Zm9vYmFy");

        check(
            "Man is distinguished, not only by his reason, but by this singular passion from "
            "other animals, which is a lust of the mind, that by a perseverance of delight "
            "in the continued and indefatigable generation of knowledge, exceeds the short "
            "vehemence of any carnal pleasure."
            ,
            "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
            "IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
            "dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
            "dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
            "ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4="
            );
    }

    void
    testSizes()
    {
        BEAST_EXPECT(base64::encoded_size(0) == 0);
        BEAST_EXPECT(base64::encoded_size(1) == 4);
        BEAST_EXPECT(base64::encoded_size(2) == 4);
        BEAST_EXPECT(base64::encoded_size(3) == 4);
        BEAST_EXPECT(base64::encoded_size(4) == 8);

        BEAST_EXPECT(base64::decoded_size(0) == 0); // r=0
        BEAST_EXPECT(base64::decoded_size(1) == 0); // r=1
        BEAST_EXPECT(base64::decoded_size(2) == 1); // r=2
        BEAST_EXPECT(base64::decoded_size(3) == 2); // r=3
        BEAST_EXPECT(base64::decoded_size(4) == 3); // r=0

        // lengths that are not a multiple of 4 must still be
        // bounded, since the caller may not know whether the
        // input was padded.
        for(std::size_t n = 0; n <= 8; ++n)
        {
            std::string const in(n, 'A'); // a valid non-pad symbol
            std::string scratch;
            scratch.resize(n + 4);
            auto const written = base64::decode(
                &scratch[0], in.data(), in.size()).first;
            BEAST_EXPECT(written <= base64::decoded_size(n));
        }
    }

    void
    run()
    {
        testRoundTrip();
        testSizes();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,base64);

} // detail
} // beast
} // boost
