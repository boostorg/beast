//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/dynamic_buffer_handle.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace detail {

class dynamic_buffer_handle_test : public beast::unit_test::suite
{
public:
    void
    testConstruction()
    {
        std::string target;
        auto handle = make_dynamic_buffer_handle(net::dynamic_buffer(target));
        using expected_type = dynamic_buffer_handle_t<decltype(net::dynamic_buffer(target))>;
        static_assert(std::is_same<decltype(handle), expected_type>::value, "incorrect handle constructed");
    }

    void
    run() override
    {
        testConstruction();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,dynamic_buffer_handle);

} // detail
} // beast
} // boost
