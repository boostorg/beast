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
#include <boost/beast/core/flat_buffer.hpp>
#include "v1_dynamic_string_buffer.hpp"

namespace boost {
namespace beast {
namespace detail {

template<class...Args>
struct can_call_make_dynamic_buffer_handle_machinery
{
    static auto test(...) -> std::false_type;

    template<class...Args2>
    static auto test(Args2&&...) -> decltype(make_dynamic_buffer_handle(std::declval<Args2>()...), void(), std::true_type());

    using type = decltype(test(std::declval<Args>()...));
};

template<class...Args>
constexpr bool
can_call_make_dynamic_buffer_handle()
{
    using truefalse = typename can_call_make_dynamic_buffer_handle_machinery<Args...>::type;
    return truefalse();
}


class dynamic_buffer_handle_test
    : public beast::unit_test::suite
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
            log << "  dynamic_buffer_handle_t<expected_type>:\n"
                   "    results in " << typeid(copy_of_dynamic_type).name()
                << "\n  expected:\n"
                   "    " << typeid(expected_type).name();
        }

        auto copied = make_dynamic_buffer_handle(handle);
        if (!BEAST_EXPECT((std::is_same<decltype(copied), decltype(handle)>::value)))
        {
            log << "  make_dynamic_buffer(dynamic_buffer_handle_t<expected_type> const&)\n"
                   "  results in " << typeid(copied).name()
                << "\n  expected:\n"
                   "    " << typeid(handle).name();
        }

        auto moved = make_dynamic_buffer_handle(std::move(handle));
        if (!BEAST_EXPECT((std::is_same<decltype(moved), decltype(handle)>::value)))
        {
            log << "  make_dynamic_buffer(dynamic_buffer_handle_t<expected_type> &&) results in:\n"
                   "    " << typeid(moved).name()
                << "\n  expected:\n"
                   "    " << typeid(handle).name();
        }
    }

    void
    testDetection()
    {
        auto target = std::string();

        // asio v1 buffer
        {
            auto v1_buffer = v1_dynamic_string_buffer(target);
            using v1_buffer_behaviour = dynamic_buffer_select_behaviour_t<decltype(v1_buffer)>;
            auto is_v1_correct = std::is_same<v1_buffer_behaviour, asio_v1_behaviour>::value;
            if (!BEAST_EXPECT(is_v1_correct))
            {
                log << "  dynamic_buffer_select_behaviour_t<v1_buffer_type> result in:\n"
                       "    " << typeid(v1_buffer_behaviour).name()
                    << "\n  expected:\n"
                       "    " << typeid(asio_v1_behaviour).name();
            }

            using dyn_buffer_type = dynamic_buffer_handle_t<decltype(v1_buffer)>;
            using expected_dyn_buffer_type = dynamic_buffer_handle<decltype(v1_buffer), asio_v1_behaviour>;

            if (!BEAST_EXPECT((std::is_same<dyn_buffer_type, expected_dyn_buffer_type>::value)))
            {
                log << "  dynamic_buffer_handle_t<v1_buffer_type> result in:\n"
                       "    " << typeid(dyn_buffer_type).name()
                    << "\n  expected:\n"
                       "    " << typeid(expected_dyn_buffer_type).name();
            }

            // construct from copy & move should be equivalent
            auto dyn_buf = make_dynamic_buffer_handle(v1_buffer);
            auto dyn_buf2 = make_dynamic_buffer_handle((std::move(v1_buffer)));
            BEAST_EXPECT((std::is_same<decltype(dyn_buf), decltype(dyn_buf2)>::value));
            BEAST_EXPECT((std::is_same<decltype(dyn_buf), expected_dyn_buffer_type>::value));
        }

        // asio v2 buffer
        {
            auto buffer = net::dynamic_buffer(target);
            using buffer_behaviour = dynamic_buffer_select_behaviour_t<decltype(buffer)>;
            auto is_correct = std::is_same<buffer_behaviour, asio_v2_behaviour>::value;
            if (!BEAST_EXPECT(is_correct))
            {
                log << "  dynamic_buffer_select_behaviour_t<buffer_type> results in:\n"
                       "    " << typeid(buffer_behaviour).name()
                    << "\n  expected:\n"
                       "    " << typeid(asio_v2_behaviour).name();
            }

            using dyn_buffer_type = dynamic_buffer_handle_t<decltype(buffer)>;
            using expected_dyn_buffer_type = dynamic_buffer_handle<decltype(buffer), asio_v2_behaviour>;

            if (!BEAST_EXPECT((std::is_same<dyn_buffer_type, expected_dyn_buffer_type>::value)))
            {
                log << "  dynamic_buffer_handle_t<buffer_type> result in:\n"
                       "    " << typeid(dyn_buffer_type).name()
                    << "\n  expected:\n"
                       "    " << typeid(expected_dyn_buffer_type).name();
            }

            // construct from copy & move should be equivalent
            auto dyn_buf = make_dynamic_buffer_handle(buffer);
            auto dyn_buf2 = make_dynamic_buffer_handle((std::move(buffer)));
            BEAST_EXPECT((std::is_same<decltype(dyn_buf), decltype(dyn_buf2)>::value));
            BEAST_EXPECT((std::is_same<decltype(dyn_buf), expected_dyn_buffer_type>::value));
        }

        // beast dynamic buffers
        {
            auto buffer = ::boost::beast::flat_buffer();
            using buffer_behaviour = dynamic_buffer_select_behaviour_t<decltype(buffer)>;
            auto is_correct = std::is_same<buffer_behaviour, beast_v1_behaviour>::value;
            if (!BEAST_EXPECT(is_correct))
            {
                log << "  dynamic_buffer_select_behaviour_t<buffer_type> results in:\n"
                       "    " << typeid(buffer_behaviour).name()
                    << "\n  expected:\n"
                       "    " << typeid(beast_v1_behaviour).name();
            }

            using dyn_buffer_type = dynamic_buffer_handle_t<decltype(buffer)>;
            using expected_dyn_buffer_type = dynamic_buffer_handle<decltype(buffer), beast_v1_behaviour>;

            if (!BEAST_EXPECT((std::is_same<dyn_buffer_type, expected_dyn_buffer_type>::value)))
            {
                log << "  dynamic_buffer_handle_t<buffer_type> result in:\n"
                       "    " << typeid(dyn_buffer_type).name()
                    << "\n  expected:\n"
                       "    " << typeid(expected_dyn_buffer_type).name();
            }

            // construct from copy & move should be equivalent
            auto dyn_buf = make_dynamic_buffer_handle(buffer);
            BEAST_EXPECT((std::is_same<decltype(dyn_buf), expected_dyn_buffer_type>::value));

            // check that dyn buffer handles can only be created from l-value mutable references to
            // Beast V1 dynamic buffers
            using buftype = ::boost::beast::flat_buffer;
            BEAST_EXPECT(can_call_make_dynamic_buffer_handle<buftype&>());
            BEAST_EXPECT(!can_call_make_dynamic_buffer_handle<buftype&&>());
            BEAST_EXPECT(!can_call_make_dynamic_buffer_handle<buftype const&>());
        }
    }

    void
    run() override
    {
        testConstruction();
        testDetection();
    }
};

BEAST_DEFINE_TESTSUITE(beast, core, dynamic_buffer_handle);

} // detail
} // beast
} // boost
