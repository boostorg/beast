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
    auto constexpr bits =
        static_cast<unsigned>(
            sizeof(T) * CHAR_BIT);
    n &= bits-1;
    return static_cast<T>((t << (bits - n)) | (
        static_cast<typename std::make_unsigned<T>::type>(t) >> n));
}

template<class = void, class T>
void
mask_inplace_fast(
    boost::asio::mutable_buffer const& b,
        T& key)
{
    auto n = b.size();
    auto p = reinterpret_cast<std::uint8_t*>(b.data());
    auto pkey = reinterpret_cast<std::uint8_t*>(&key);

    size_t ikey = 0;
    for (size_t i = 0; i < n; i++)
    {
        p[i] ^= pkey[ikey++];
        ikey %= sizeof(key);
    }

    auto oldkey = key;
    auto poldkey = reinterpret_cast<std::uint8_t*>(&oldkey);
    for (size_t i = 0; i < sizeof(key); i++)
    {
        pkey[i] = poldkey[ikey++];
        ikey %= sizeof(key);
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
