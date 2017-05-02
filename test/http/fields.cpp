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

    void testHeaders()
    {
        f_t f1;
        BEAST_EXPECT(f1.empty());
        fill(1, f1);
        BEAST_EXPECT(f1.size() == 1);
        f_t f2;
        f2 = f1;
        BEAST_EXPECT(f2.size() == 1);
        f2.insert("2", "2");
        BEAST_EXPECT(std::distance(f2.begin(), f2.end()) == 2);
        f1 = std::move(f2);
        BEAST_EXPECT(f1.size() == 2);
        BEAST_EXPECT(f2.size() == 0);
        f_t f3(std::move(f1));
        BEAST_EXPECT(f3.size() == 2);
        BEAST_EXPECT(f1.size() == 0);
        self_assign(f3, std::move(f3));
        BEAST_EXPECT(f3.size() == 2);
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
        BEAST_EXPECT(f.size() == 4);
        f.erase("a");
        BEAST_EXPECT(f.size() == 2);
    }

    void run() override
    {
        testHeaders();
        testRFC2616();
    }
};

BEAST_DEFINE_TESTSUITE(basic_fields,http,beast);

} // http
} // beast
