//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_PRNG_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_PRNG_HPP

#include <boost/beast/core/detail/config.hpp>
#include <cstdint>
#include <limits>
#include <random>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// Type-erased UniformRandomBitGenerator
// with 32-bit unsigned integer value_type
//
class prng
{
protected:
    virtual ~prng() = default;
    virtual prng& acquire() noexcept = 0;
    virtual void release() noexcept = 0;
    virtual std::uint32_t operator()() noexcept = 0;

public:
    using value_type = std::uint32_t;

    class ref
    {
        prng& p_;

    public:
        ref& operator=(ref const&) = delete;

        ~ref()
        {
            p_.release();
        }

        explicit
        ref(prng& p) noexcept
            : p_(p.acquire())
        {
        }

        ref(ref const& other) noexcept
            : p_(other.p_.acquire())
        {
        }

        value_type
        operator()() noexcept
        {
            return p_();
        }

        static
        value_type constexpr
        min() noexcept
        {
            return (std::numeric_limits<
                value_type>::min)();
        }

        static
        value_type constexpr
        max() noexcept
        {
            return (std::numeric_limits<
                value_type>::max)();
        }
    };
};

//------------------------------------------------------------------------------

// Manually seed the prngs, must be called
// before acquiring a prng for the first time.
//
BOOST_BEAST_DECL
std::uint32_t const*
prng_seed(std::seed_seq* ss = nullptr);

// Acquire a PRNG using the no-TLS implementation
//
BOOST_BEAST_DECL
prng::ref
make_prng_no_tls(bool secure);

// Acquire a PRNG using the TLS implementation
//
#ifndef BOOST_NO_CXX11_THREAD_LOCAL
BOOST_BEAST_DECL
prng::ref
make_prng_tls(bool secure);
#endif

// Acquire a PRNG using the TLS implementation if it
// is available, otherwise using the no-TLS implementation.
//
BOOST_BEAST_DECL
prng::ref
make_prng(bool secure);

} // detail
} // websocket
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/websocket/detail/prng.ipp>
#endif

#endif
