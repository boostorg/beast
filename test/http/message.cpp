//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/message.hpp>

#include <beast/http/string_body.hpp>
#include <beast/http/fields.hpp>
#include <beast/http/string_body.hpp>
#include <beast/unit_test/suite.hpp>
#include <type_traits>

namespace beast {
namespace http {

class message_test : public beast::unit_test::suite
{
public:
    struct Arg1
    {
        bool moved = false;

        Arg1() = default;

        Arg1(Arg1&& other)
        {
            other.moved = true;
        }
    };

    struct Arg2 { };
    struct Arg3 { };

    // default constructible Body
    struct default_body
    {
        using value_type = std::string;
    };

    // 1-arg constructible Body
    struct one_arg_body
    {
        struct value_type
        {
            explicit
            value_type(Arg1 const&)
            {
            }

            explicit
            value_type(Arg1&& arg)
            {
                Arg1 arg_(std::move(arg));
            }
        };
    };

    // 2-arg constructible Body
    struct two_arg_body
    {
        struct value_type
        {
            value_type(Arg1 const&, Arg2 const&)
            {
            }
        };
    };

    void
    testMessage()
    {
        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, default_body, fields>>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, one_arg_body, fields>, Arg1>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, one_arg_body, fields>, Arg1 const>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, one_arg_body, fields>, Arg1 const&>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, one_arg_body, fields>, Arg1&&>::value);

        BOOST_STATIC_ASSERT(! std::is_constructible<
            message<true, one_arg_body, fields>>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, one_arg_body, fields>,
                Arg1, fields::allocator_type>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, one_arg_body, fields>, std::piecewise_construct_t,
                std::tuple<Arg1>>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, two_arg_body, fields>, std::piecewise_construct_t,
                std::tuple<Arg1, Arg2>>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            message<true, two_arg_body, fields>, std::piecewise_construct_t,
                std::tuple<Arg1, Arg2>, std::tuple<fields::allocator_type>>::value);

        {
            Arg1 arg1;
            message<true, one_arg_body, fields>{std::move(arg1)};
            BEAST_EXPECT(arg1.moved);
        }

        {
            fields h;
            h.insert("User-Agent", "test");
            message<true, one_arg_body, fields> m{Arg1{}, h};
            BEAST_EXPECT(h["User-Agent"] == "test");
            BEAST_EXPECT(m.fields["User-Agent"] == "test");
        }
        {
            fields h;
            h.insert("User-Agent", "test");
            message<true, one_arg_body, fields> m{Arg1{}, std::move(h)};
            BEAST_EXPECT(! h.exists("User-Agent"));
            BEAST_EXPECT(m.fields["User-Agent"] == "test");
        }

        // swap
        message<true, string_body, fields> m1;
        message<true, string_body, fields> m2;
        m1.target("u");
        m1.body = "1";
        m1.fields.insert("h", "v");
        m2.method("G");
        m2.body = "2";
        swap(m1, m2);
        BEAST_EXPECT(m1.method_string() == "G");
        BEAST_EXPECT(m2.method_string().empty());
        BEAST_EXPECT(m1.target().empty());
        BEAST_EXPECT(m2.target() == "u");
        BEAST_EXPECT(m1.body == "2");
        BEAST_EXPECT(m2.body == "1");
        BEAST_EXPECT(! m1.fields.exists("h"));
        BEAST_EXPECT(m2.fields.exists("h"));
    }

    struct MoveFields : fields
    {
        bool moved_to = false;
        bool moved_from = false;

        MoveFields() = default;

        MoveFields(MoveFields&& other)
            : moved_to(true)
        {
            other.moved_from = true;
        }

        MoveFields& operator=(MoveFields&& other)
        {
            return *this;
        }
    };

    void
    testHeaders()
    {
        {
            using req_type = header<true>;
            BOOST_STATIC_ASSERT(std::is_copy_constructible<req_type>::value);
            BOOST_STATIC_ASSERT(std::is_move_constructible<req_type>::value);
            BOOST_STATIC_ASSERT(std::is_copy_assignable<req_type>::value);
            BOOST_STATIC_ASSERT(std::is_move_assignable<req_type>::value);

            using res_type = header<false>;
            BOOST_STATIC_ASSERT(std::is_copy_constructible<res_type>::value);
            BOOST_STATIC_ASSERT(std::is_move_constructible<res_type>::value);
            BOOST_STATIC_ASSERT(std::is_copy_assignable<res_type>::value);
            BOOST_STATIC_ASSERT(std::is_move_assignable<res_type>::value);
        }

        {
            MoveFields h;
            header<true, MoveFields> r{std::move(h)};
            BEAST_EXPECT(h.moved_from);
            BEAST_EXPECT(r.fields.moved_to);
            request<string_body, MoveFields> m{std::move(r)};
            BEAST_EXPECT(r.fields.moved_from);
            BEAST_EXPECT(m.fields.moved_to);
        }
    }

    void
    testFreeFunctions()
    {
        {
            request<string_body> m;
            m.method(verb::get);
            m.target("/");
            m.version = 11;
            m.fields.insert("Upgrade", "test");
            BEAST_EXPECT(! is_upgrade(m));

            prepare(m, connection::upgrade);
            BEAST_EXPECT(is_upgrade(m));
            BEAST_EXPECT(m.fields["Connection"] == "upgrade");

            m.version = 10;
            BEAST_EXPECT(! is_upgrade(m));
        }
    }

    void
    testPrepare()
    {
        request<string_body> m;
        m.version = 10;
        BEAST_EXPECT(! is_upgrade(m));
        m.fields.insert("Transfer-Encoding", "chunked");
        try
        {
            prepare(m);
            fail();
        }
        catch(std::exception const&)
        {
        }
        m.fields.erase("Transfer-Encoding");
        m.fields.insert("Content-Length", "0");
        try
        {
            prepare(m);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
        m.fields.erase("Content-Length");
        m.fields.insert("Connection", "keep-alive");
        try
        {
            prepare(m);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
        m.version = 11;
        m.fields.erase("Connection");
        m.fields.insert("Connection", "close");
        BEAST_EXPECT(! is_keep_alive(m));
    }

    void
    testSwap()
    {
        message<false, string_body, fields> m1;
        message<false, string_body, fields> m2;
        m1.result(status::ok);
        m1.version = 10;
        m1.body = "1";
        m1.fields.insert("h", "v");
        m2.result(status::not_found);
        m2.body = "2";
        m2.version = 11;
        swap(m1, m2);
        BEAST_EXPECT(m1.result() == status::not_found);
        BEAST_EXPECT(m1.result_int() == 404);
        BEAST_EXPECT(m2.result() == status::ok);
        BEAST_EXPECT(m2.result_int() == 200);
        BEAST_EXPECT(m1.reason() == "Not Found");
        BEAST_EXPECT(m2.reason() == "OK");
        BEAST_EXPECT(m1.version == 11);
        BEAST_EXPECT(m2.version == 10);
        BEAST_EXPECT(m1.body == "2");
        BEAST_EXPECT(m2.body == "1");
        BEAST_EXPECT(! m1.fields.exists("h"));
        BEAST_EXPECT(m2.fields.exists("h"));
    }

    void
    testSpecialMembers()
    {
        response<string_body> r1;
        response<string_body> r2{r1};
        response<string_body> r3{std::move(r2)};
        r2 = r3;
        r1 = std::move(r2);
        [r1]()
        {
        }();
    }

    void
    testMethod()
    {
        header<true> h;
        auto const vcheck =
            [&](verb v)
            {
                h.method(v);
                BEAST_EXPECT(h.method() == v);
                BEAST_EXPECT(h.method_string() == to_string(v));
            };
        auto const scheck =
            [&](string_view s)
            {
                h.method(s);
                BEAST_EXPECT(h.method() == string_to_verb(s));
                BEAST_EXPECT(h.method_string() == s);
            };
        vcheck(verb::get);
        vcheck(verb::head);
        scheck("GET");
        scheck("HEAD");
        scheck("XYZ");
    }

    void
    testStatus()
    {
        header<false> h;
        h.result(200);
        BEAST_EXPECT(h.result_int() == 200);
        BEAST_EXPECT(h.result() == status::ok);
        h.result(status::switching_protocols);
        BEAST_EXPECT(h.result_int() == 101);
        BEAST_EXPECT(h.result() == status::switching_protocols);
        h.result(1);
        BEAST_EXPECT(h.result_int() == 1);
        BEAST_EXPECT(h.result() == status::unknown);
    }

    void
    testReason()
    {
        header<false> h;
        h.result(status::ok);
        BEAST_EXPECT(h.reason() == "OK");
        h.reason("Pepe");
        BEAST_EXPECT(h.reason() == "Pepe");
        h.result(status::not_found);
        BEAST_EXPECT(h.reason() == "Pepe");
        h.reason({});
        BEAST_EXPECT(h.reason() == "Not Found");
    }

    void
    run() override
    {
        testMessage();
        testHeaders();
        testFreeFunctions();
        testPrepare();
        testSwap();
        testSpecialMembers();
        testMethod();
        testStatus();
        testReason();
    }
};

BEAST_DEFINE_TESTSUITE(message,http,beast);

} // http
} // beast
