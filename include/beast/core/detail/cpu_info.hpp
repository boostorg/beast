//
// Copyright (c) 2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_DETAIL_CPU_INFO_HPP
#define BEAST_DETAIL_CPU_INFO_HPP

#ifndef BEAST_NO_INTRINSICS
#include <boost/predef/hardware/simd.h>
# if (BOOST_HW_SIMD_X86 >= BOOST_HW_SIMD_X86_SSE4_2_VERSION)
#  define BEAST_NO_INTRINSICS 0
# else
#  define BEAST_NO_INTRINSICS 1
# endif
#endif

#if (!BEAST_NO_INTRINSICS)
#ifdef _MSC_VER
#include <intrin.h> // __cpuid
#else
#include <cpuid.h>  // __get_cpuid
#endif
#endif

namespace beast {
namespace detail {

/*  Portions from Boost,
    Copyright Andrey Semashev 2007 - 2015.
*/
#if (!BEAST_NO_INTRINSICS)

template<class = void>
void
cpuid(
    std::uint32_t id,
    std::uint32_t& eax,
    std::uint32_t& ebx,
    std::uint32_t& ecx,
    std::uint32_t& edx)
{
#ifdef _MSC_VER
    int regs[4];
    __cpuid(regs, id);
    eax = regs[0];
    ebx = regs[1];
    ecx = regs[2];
    edx = regs[3];
#else
    __get_cpuid(id,  &eax,  &ebx,  &ecx, &edx);
#endif
}

#endif

struct cpu_info
{
    bool sse42 = false;

    cpu_info();
};

inline
cpu_info::
cpu_info()
{
#if (!BEAST_NO_INTRINSICS)
    constexpr std::uint32_t SSE42 = 1 << 20;
    std::uint32_t eax, ebx, ecx, edx;

    cpuid(0, eax, ebx, ecx, edx);
    if(eax >= 1)
    {
        cpuid(1, eax, ebx, ecx, edx);
        sse42 = (ecx & SSE42) != 0;
    }
#endif
}

template<class = void>
cpu_info const&
get_cpu_info()
{
    static cpu_info const ci;
    return ci;
}

} // detail
} // beast

#endif
