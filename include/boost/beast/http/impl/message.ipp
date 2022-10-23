//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/beast/http/message.hpp>

#ifndef BOOST_BEAST_HTTP_MESSAGE_IPP
#define BOOST_BEAST_HTTP_MESSAGE_IPP

namespace boost {
namespace beast {
namespace http {

#if defined(BOOST_BEAST_SOURCE)
template class header<true, fields>;
template class header<false, fields>;
#endif

}
}
}

#endif //BOOST_BEAST_HTTP_MESSAGE_IPP
