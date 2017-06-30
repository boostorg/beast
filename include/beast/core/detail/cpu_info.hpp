//
// Copyright (c) 2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_DETAIL_CPU_INFO_HPP
#define BEAST_DETAIL_CPU_INFO_HPP

#ifndef BEAST_NO_INTRINSICS
# if defined(_MSC_VER) || \
    (defined(__i386__) && defined(__PIC__) && \
     defined(__GNUC__) && ! defined(__clang__)) || \
    defined(__i386__)
#  define BEAST_NO_INTRINSICS 0
# else
#  define BEAST_NO_INTRINSICS 1
# endif
#endif

#if ! BEAST_NO_INTRINSICS

#if defined(_MSC_VER)
#include <intrin.h> // __cpuid
#endif

namespace beast {
namespace detail {

/*  Portions from Boost,
    Copyright Andrey Semashev 2007 - 2015.
*/
template<class = void>
void
cpuid(
    std::uint32_t id,
    std::uint32_t& eax,
    std::uint32_t& ebx,
    std::uint32_t& ecx,
    std::uint32_t& edx)
{
#if defined(_MSC_VER)
    int regs[4];
    __cpuid(regs, id);
    eax = regs[0];
    ebx = regs[1];
    ecx = regs[2];
    edx = regs[3];

#elif defined(__i386__) && defined(__PIC__) && \
      defined(__GNUC__) && ! defined(__clang__)
    // We have to backup ebx in 32 bit PIC code because it is reserved by the ABI
    uint32_t ebx_backup;
    __asm__ __volatile__
    (
        "movl %%ebx, %0\n\t"
        "movl %1, %%ebx\n\t"
        "cpuid\n\t"
        "movl %%ebx, %1\n\t"
        "movl %0, %%ebx\n\t"
            : "=m" (ebx_backup), "+m"(ebx), "+a"(eax), "+c"(ecx), "+d"(edx)
    );

#elif defined(__i386__)
    __asm__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(id) : "ebx");

#else
# error Unknown compiler!

#endif
}

struct cpu_info
{
    bool sse42 = false;

    cpu_info();
};

inline
cpu_info::
cpu_info()
{
    std::uint32_t eax, ebx, ecx, edx;

    cpuid(0, eax, ebx, ecx, edx);
    if(eax >= 1)
    {
        cpuid(1, eax, ebx, ecx, edx);
        sse42 = (ecx & 20) != 0;
    }
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

#endif
