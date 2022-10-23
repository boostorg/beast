//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_IMPL_TCP_STREAM_IPP
#define BEAST_IMPL_TCP_STREAM_IPP

#include <boost/beast/core/tcp_stream.hpp>

namespace boost
{
namespace beast
{

template class basic_stream<
    net::ip::tcp,
    net::any_io_executor,
    unlimited_rate_policy>;

}
}

#endif //BEAST_IMPL_TCP_STREAM_IPP
