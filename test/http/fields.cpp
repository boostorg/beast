//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/fields.hpp>

#include <beast/unit_test/suite.hpp>
#include <boost/lexical_cast.hpp>

namespace beast {
namespace http {

class basic_fields_test : public beast::unit_test::suite
{
public:
    template<class Allocator>
    using fa_t = basic_fields<Allocator>;

    using f_t = fa_t<std::allocator<char>>;

    template<class Allocator>
    static
    void
    fill(std::size_t n, basic_fields<Allocator>& f)
    {
        for(std::size_t i = 1; i<= n; ++i)
            f.insert(boost::lexical_cast<std::string>(i), i);
    }

    template<class U, class V>
    static
    void
    self_assign(U& u, V&& v)
    {
        u = std::forward<V>(v);
    }

    template<class Alloc>
    static
    bool
    empty(basic_fields<Alloc> const& f)
    {
        return f.begin() == f.end();
    }

    template<class Alloc>
    static
    std::size_t
    size(basic_fields<Alloc> const& f)
    {
        return std::distance(f.begin(), f.end());
    }

    void testHeaders()
    {
        f_t f1;
        BEAST_EXPECT(empty(f1));
        fill(1, f1);
        BEAST_EXPECT(size(f1) == 1);
        f_t f2;
        f2 = f1;
        BEAST_EXPECT(size(f2) == 1);
        f2.insert("2", "2");
        BEAST_EXPECT(std::distance(f2.begin(), f2.end()) == 2);
        f1 = std::move(f2);
        BEAST_EXPECT(size(f1) == 2);
        BEAST_EXPECT(size(f2) == 0);
        f_t f3(std::move(f1));
        BEAST_EXPECT(size(f3) == 2);
        BEAST_EXPECT(size(f1) == 0);
        self_assign(f3, std::move(f3));
        BEAST_EXPECT(size(f3) == 2);
        BEAST_EXPECT(f2.erase("Not-Present") == 0);
    }

    void testRFC2616()
    {
        f_t f;
        f.insert("a", "w");
        f.insert("a", "x");
        f.insert("aa", "y");
        f.insert("b", "z");
        BEAST_EXPECT(f.count("a") == 2);
    }

    void testErase()
    {
        f_t f;
        f.insert("a", "w");
        f.insert("a", "x");
        f.insert("aa", "y");
        f.insert("b", "z");
        BEAST_EXPECT(size(f) == 4);
        f.erase("a");
        BEAST_EXPECT(size(f) == 2);
    }

    void
    testMethodString()
    {
        f_t f;
        f.method_string("CRY");
        BEAST_EXPECTS(f.method_string() == "CRY", f.method_string());
        f.method_string("PUT");
        BEAST_EXPECTS(f.method_string() == "PUT", f.method_string());
        f.method_string({});
        BEAST_EXPECTS(f.method_string().empty(), f.method_string());
    }

    void run() override
    {
        testHeaders();
        testRFC2616();
        testErase();
        testMethodString();
    }
};

BEAST_DEFINE_TESTSUITE(basic_fields,http,beast);

} // http
} // beast
