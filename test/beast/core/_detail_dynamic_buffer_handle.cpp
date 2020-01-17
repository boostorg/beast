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
#include <boost/type_index.hpp>

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

        // construct current version asio buffer
        auto handle = make_dynamic_buffer_handle(net::dynamic_buffer(target));
        using expected_type = dynamic_buffer_handle_t<decltype(net::dynamic_buffer(target))>;
        BEAST_EXPECT((std::is_same<decltype(handle), expected_type>::value));
        BEAST_EXPECT(is_dynamic_buffer_handle<expected_type>::value);

        // constructing dynamic_buffer from dynamic_buffer should result in a copy
        using copy_of_dynamic_type = dynamic_buffer_handle_t<expected_type>;
        if (!BEAST_EXPECT((std::is_same<expected_type, copy_of_dynamic_type>::value)))
        {
            log << "  dynamic_buffer_handle_t<expected_type> results in "
                << boost::typeindex::type_id<copy_of_dynamic_type>().pretty_name()
                << "\n  expected: " << boost::typeindex::type_id<expected_type>().pretty_name();
        }

        auto copied = make_dynamic_buffer_handle(handle);
        if (!BEAST_EXPECT((std::is_same<decltype(copied), decltype(handle)>::value)))
        {
            log << "  make_dynamic_buffer(dynamic_buffer_handle_t<expected_type> const&) results in "
                << boost::typeindex::type_id<decltype(copied)>().pretty_name()
                << "\n  expected: " << boost::typeindex::type_id<decltype(handle)>().pretty_name();
        }

        auto moved = make_dynamic_buffer_handle(std::move(handle));
        if (!BEAST_EXPECT((std::is_same<decltype(moved), decltype(handle)>::value)))
        {
            log << "  make_dynamic_buffer(dynamic_buffer_handle_t<expected_type> &&) results in "
                << boost::typeindex::type_id<decltype(moved)>().pretty_name()
                << "\n  expected: " << boost::typeindex::type_id<decltype(handle)>().pretty_name();
        }
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
