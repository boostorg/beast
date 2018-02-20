//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_MASK_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_MASK_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <array>
#include <climits>
#include <cstdint>
#include <random>
#include <type_traits>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// Pseudo-random source of mask keys
//
template<class Generator>
class maskgen_t
{
    Generator g_;

public:
    using result_type =
        typename Generator::result_type;

    maskgen_t();

    result_type
    operator()() noexcept;

    void
    rekey();
};

template<class Generator>
maskgen_t<Generator>::maskgen_t()
{
    rekey();
}

template<class Generator>
auto
maskgen_t<Generator>::operator()() noexcept ->
    result_type
{
    for(;;)
        if(auto key = g_())
            return key;
}

template<class _>
void
maskgen_t<_>::rekey()
{
    std::random_device rng;
#if 0
    std::array<std::uint32_t, 32> e;
    for(auto& i : e)
        i = rng();
    // VFALCO This constructor causes
    //        address sanitizer to fail, no idea why.
    std::seed_seq ss(e.begin(), e.end());
    g_.seed(ss);
#else
    g_.seed(rng());
#endif
}

// VFALCO NOTE This generator has 5KB of state!
//using maskgen = maskgen_t<std::mt19937>;
using maskgen = maskgen_t<std::minstd_rand>;

//------------------------------------------------------------------------------

using prepared_key =
    std::conditional<sizeof(void*) == 8,
        std::uint64_t, std::uint32_t>::type;

inline
void
prepare_key(std::uint32_t& prepared, std::uint32_t key)
{
    prepared = key;
}

inline
void
prepare_key(std::uint64_t& prepared, std::uint32_t key)
{
    prepared =
        (static_cast<std::uint64_t>(key) << 32) | key;
}

template<class T>
inline
typename std::enable_if<std::is_integral<T>::value, T>::type
ror(T t, unsigned n = 1)
{
  const T mask = (CHAR_BIT*sizeof(t) - 1);
  n &= mask;
  return static_cast<T>((t << ( (-n)&mask )) | (
        static_cast<typename std::make_unsigned<T>::type>(t) >> n));
}

template<class T>
inline
typename std::enable_if<std::is_integral<T>::value, T>::type
rol(T t, unsigned n = 1)
{
  const T mask = (CHAR_BIT*sizeof(t) - 1);
  n &= mask;
  return static_cast<T>((t >> ( (-n)&mask )) | (
        static_cast<typename std::make_unsigned<T>::type>(t) << n));
}

template<class T>
inline
typename std::enable_if<std::is_integral<T>::value, T>::type
rotate_key(T t, unsigned n_uint8_t)
{
#   ifdef BOOST_BIG_ENDIAN
    return rol(t, n_uint8_t * CHAR_BIT);
#   else
    return ror(t, n_uint8_t * CHAR_BIT);
#   endif
}

template<class T>
inline
typename std::enable_if<std::is_integral<T>::value, T>::type
byte_at(T t, unsigned n)
{
#   ifdef BOOST_BIG_ENDIAN
    return static_cast<std::uint8_t>(n >> (CHAR_BIT * (sizeof(T) - n - 1)));
#   else
    return static_cast<std::uint8_t>(n >> (CHAR_BIT * n));
#   endif
}

// 32-bit optimized
//
template<class = void>
void
mask_inplace_fast(
    boost::asio::mutable_buffer const& b,
        std::uint32_t& key)
{
    auto n = b.size();
    auto p = reinterpret_cast<std::uint8_t*>(b.data());
    if(n >= sizeof(key))
    {
        // Bring p to 4-byte alignment
        auto const i = reinterpret_cast<
            std::uintptr_t>(p) & (sizeof(key)-1);
        switch(i)
        {
        case 1: p[2] ^= byte_at(key, 2); BOOST_FALLTHROUGH;
        case 2: p[1] ^= byte_at(key, 1); BOOST_FALLTHROUGH;
        case 3: p[0] ^= byte_at(key, 0);
        {
            auto const d = static_cast<unsigned>(sizeof(key) - i);
            key = rotate_key(key, d);
            n -= d;
            p += d;
            BOOST_FALLTHROUGH;
        }
        default:
            break;
        }
    }

    // Mask 4 bytes at a time
    for(auto i = n / sizeof(key); i; --i)
    {
        *reinterpret_cast<
            std::uint32_t*>(p) ^= key;
        p += sizeof(key);
    }

    // Leftovers
    n &= sizeof(key)-1;
    switch(n)
    {
    case 3: p[2] ^= byte_at(key, 2); BOOST_FALLTHROUGH;
    case 2: p[1] ^= byte_at(key, 1); BOOST_FALLTHROUGH;
    case 1: p[0] ^= byte_at(key, 0);
        key = rotate_key(key, static_cast<unsigned>(n));
        BOOST_FALLTHROUGH;
    default:
        break;
    }
}

// 64-bit optimized
//
template<class = void>
void
mask_inplace_fast(
    boost::asio::mutable_buffer const& b,
        std::uint64_t& key)
{
    auto n = b.size();
    auto p = reinterpret_cast<std::uint8_t*>(b.data());
    if(n >= sizeof(key))
    {
        // Bring p to 8-byte alignment
        auto const i = reinterpret_cast<
            std::uintptr_t>(p) & (sizeof(key)-1);
        switch(i)
        {
        case 1: p[6] ^= byte_at(key, 6); BOOST_FALLTHROUGH;
        case 2: p[5] ^= byte_at(key, 5); BOOST_FALLTHROUGH;
        case 3: p[4] ^= byte_at(key, 4); BOOST_FALLTHROUGH;
        case 4: p[3] ^= byte_at(key, 3); BOOST_FALLTHROUGH;
        case 5: p[2] ^= byte_at(key, 2); BOOST_FALLTHROUGH;
        case 6: p[1] ^= byte_at(key, 1); BOOST_FALLTHROUGH;
        case 7: p[0] ^= byte_at(key, 0);
        {
            auto const d = static_cast<
                unsigned>(sizeof(key) - i);
            key = rotate_key(key, d);
            n -= d;
            p += d;
            BOOST_FALLTHROUGH;
        }
        default:
            break;
        }
    }

    // Mask 8 bytes at a time
    for(auto i = n / sizeof(key); i; --i)
    {
        *reinterpret_cast<
            std::uint64_t*>(p) ^= key;
        p += sizeof(key);
    }

    // Leftovers
    n &= sizeof(key)-1;
    switch(n)
    {
    case 7: p[6] ^= byte_at(key, 6); BOOST_FALLTHROUGH;
    case 6: p[5] ^= byte_at(key, 5); BOOST_FALLTHROUGH;
    case 5: p[4] ^= byte_at(key, 4); BOOST_FALLTHROUGH;
    case 4: p[3] ^= byte_at(key, 3); BOOST_FALLTHROUGH;
    case 3: p[2] ^= byte_at(key, 2); BOOST_FALLTHROUGH;
    case 2: p[1] ^= byte_at(key, 1); BOOST_FALLTHROUGH;
    case 1: p[0] ^= byte_at(key, 0);
        key = rotate_key(key, static_cast<unsigned>(n));
        BOOST_FALLTHROUGH;
    default:
        break;
    }
}

inline
void
mask_inplace(
    boost::asio::mutable_buffer const& b,
        std::uint32_t& key)
{
    mask_inplace_fast(b, key);
}

inline
void
mask_inplace(
    boost::asio::mutable_buffer const& b,
        std::uint64_t& key)
{
    mask_inplace_fast(b, key);
}

// Apply mask in place
//
template<class MutableBuffers, class KeyType>
void
mask_inplace(
    MutableBuffers const& bs, KeyType& key)
{
    for(boost::asio::mutable_buffer b : bs)
        mask_inplace(b, key);
}

} // detail
} // websocket
} // beast
} // boost

#endif
