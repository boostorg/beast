//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/beast/http/dynamic_body.hpp>

#ifndef BOOST_BEAST_HTTP_DYNAMIC_BODY_IPP
#define BOOST_BEAST_HTTP_DYNAMIC_BODY_IPP

namespace boost {
namespace beast {
namespace http {

template struct basic_dynamic_body<multi_buffer>;

}
}
}

#endif //BOOST_BEAST_HTTP_DYNAMIC_BODY_IPP
