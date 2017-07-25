//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/variant.hpp>

#include <boost/beast/unit_test/suite.hpp>
#include <string>

namespace boost {
namespace beast {
namespace detail {

class variant_test : public beast::unit_test::suite
{
public:
    template<unsigned char I>
    struct Q
    {
        static
        unsigned char&
        count()
        {
            static unsigned char n = 0;
            return n;
        }

        bool move = false;
        bool copy = false;
        bool move_assign = false;
        bool copy_assign = false;

        Q() { ++count(); }
        ~Q() { --count(); }
        Q(Q&&) { ++count(); move = true; }
        Q(Q const&) { ++count(); copy = true; }
        Q& operator=(Q&&) { move_assign = true; return *this; }
        Q& operator=(Q const&) { copy_assign = true; return *this; }
    };

    void
    testVariant()
    {
        // Default Constructor
        // Destructor
        {
            variant<int> v;
            BEAST_EXPECT(v.index() == 0);
        }
        // emplace
        {
            variant<int> v;
            BEAST_EXPECT(v.index() == 0);
            v.emplace<1>(10);
            BEAST_EXPECT(v.index() == 1);
            BEAST_EXPECT(v.get<1>() == 10);
        }
        {
            variant<std::string> v;
            v.emplace<1>("Hello, world!");
            BEAST_EXPECT(v.index() == 1);
            BEAST_EXPECT(v.get<1>() == "Hello, world!");
        }
        {
            variant<Q<1>> v;
            BEAST_EXPECT(Q<1>::count() == 0);
            v.emplace<1>();
            BEAST_EXPECT(Q<1>::count() == 1);
            v.reset();
            BEAST_EXPECT(Q<1>::count() == 0);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        {
            variant<Q<1>, Q<2>, Q<3>> v;
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 0);
            BEAST_EXPECT(Q<3>::count() == 0);
            v.emplace<1>();
            BEAST_EXPECT(Q<1>::count() == 1);
            BEAST_EXPECT(Q<2>::count() == 0);
            BEAST_EXPECT(Q<3>::count() == 0);
            v.emplace<2>();
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 1);
            BEAST_EXPECT(Q<3>::count() == 0);
            v.emplace<3>();
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 0);
            BEAST_EXPECT(Q<3>::count() == 1);
            v.reset();
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 0);
            BEAST_EXPECT(Q<3>::count() == 0);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        BEAST_EXPECT(Q<2>::count() == 0);
        BEAST_EXPECT(Q<3>::count() == 0);
        // move
        {
            variant<std::string> v1;
            BEAST_EXPECT(v1.index() == 0);
            variant<std::string> v2{std::move(v1)};
            BEAST_EXPECT(v1.index() == 0);
            BEAST_EXPECT(v2.index() == 0);
            v2.emplace<1>("Hello");
            BEAST_EXPECT(v2.get<1>() == "Hello");
            variant<std::string> v3{std::move(v2)};
            BEAST_EXPECT(v2.index() == 0);
            BEAST_EXPECT(v3.get<1>() == "Hello");
        }
        {
            variant<Q<1>> v1;
            BEAST_EXPECT(Q<1>::count() == 0);
            v1.emplace<1>();
            BEAST_EXPECT(Q<1>::count() == 1);
            BEAST_EXPECT(! v1.get<1>().move);
            variant<Q<1>> v2{std::move(v1)};
            BEAST_EXPECT(v1.index() == 0);
            BEAST_EXPECT(Q<1>::count() == 1);
            BEAST_EXPECT(v2.get<1>().move);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        {
            variant<Q<1>, Q<2>, Q<3>> v1;
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 0);
            BEAST_EXPECT(Q<3>::count() == 0);
            v1.emplace<2>();
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 1);
            BEAST_EXPECT(Q<3>::count() == 0);
            variant<Q<1>, Q<2>, Q<3>> v2{std::move(v1)};
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 1);
            BEAST_EXPECT(Q<3>::count() == 0);
            BEAST_EXPECT(v2.get<2>().move);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        BEAST_EXPECT(Q<2>::count() == 0);
        BEAST_EXPECT(Q<3>::count() == 0);
        // copy
        {
            variant<std::string> v1;
            BEAST_EXPECT(v1.index() == 0);
            variant<std::string> v2{v1};
            BEAST_EXPECT(v1.index() == 0);
            BEAST_EXPECT(v2.index() == 0);
            v2.emplace<1>("Hello");
            BEAST_EXPECT(v2.get<1>() == "Hello");
            variant<std::string> v3{v2};
            BEAST_EXPECT(v2.get<1>() == "Hello");
            BEAST_EXPECT(v3.get<1>() == "Hello");
        }
        {
            variant<Q<1>> v1;
            BEAST_EXPECT(Q<1>::count() == 0);
            v1.emplace<1>();
            BEAST_EXPECT(Q<1>::count() == 1);
            BEAST_EXPECT(! v1.get<1>().copy);
            variant<Q<1>> v2{v1};
            BEAST_EXPECT(v1.index() == 1);
            BEAST_EXPECT(Q<1>::count() == 2);
            BEAST_EXPECT(v2.get<1>().copy);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        {
            variant<Q<1>, Q<2>, Q<3>> v1;
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 0);
            BEAST_EXPECT(Q<3>::count() == 0);
            v1.emplace<2>();
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 1);
            BEAST_EXPECT(Q<3>::count() == 0);
            variant<Q<1>, Q<2>, Q<3>> v2{v1};
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 2);
            BEAST_EXPECT(Q<3>::count() == 0);
            BEAST_EXPECT(v2.get<2>().copy);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        BEAST_EXPECT(Q<2>::count() == 0);
        BEAST_EXPECT(Q<3>::count() == 0);
        // move assign
        {
            variant<std::string> v1;
            BEAST_EXPECT(v1.index() == 0);
            variant<std::string> v2;
            v2 = std::move(v1);
            BEAST_EXPECT(v1.index() == 0);
            BEAST_EXPECT(v2.index() == 0);
            v2.emplace<1>("Hello");
            BEAST_EXPECT(v2.get<1>() == "Hello");
            variant<std::string> v3;
            v3 = std::move(v2);
            BEAST_EXPECT(v2.index() == 0);
            BEAST_EXPECT(v3.get<1>() == "Hello");
        }
        {
            variant<Q<1>> v1;
            v1.emplace<1>();
            BEAST_EXPECT(! v1.get<1>().move_assign);
            BEAST_EXPECT(Q<1>::count() == 1);
            variant<Q<1>> v2;
            v2.emplace<1>();
            BEAST_EXPECT(Q<1>::count() == 2);
            BEAST_EXPECT(! v2.get<1>().move_assign);
            v2 = std::move(v1);
            BEAST_EXPECT(v1.index() == 0);
            BEAST_EXPECT(v2.index() == 1);
            BEAST_EXPECT(v2.get<1>().move);
            BEAST_EXPECT(!v2.get<1>().move_assign);
            BEAST_EXPECT(Q<1>::count() == 1);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        {
            variant<Q<1>, Q<2>, Q<3>> v1;
            v1.emplace<2>();
            BEAST_EXPECT(! v1.get<2>().move_assign);
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 1);
            BEAST_EXPECT(Q<3>::count() == 0);
            variant<Q<1>, Q<2>, Q<3>> v2;
            v2.emplace<2>();
            BEAST_EXPECT(! v2.get<2>().move_assign);
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 2);
            BEAST_EXPECT(Q<3>::count() == 0);
            v2 = std::move(v1);
            BEAST_EXPECT(v1.index() == 0);
            BEAST_EXPECT(v2.index() == 2);
            BEAST_EXPECT(! v2.get<2>().move_assign);
            BEAST_EXPECT(v2.get<2>().move);
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 1);
            BEAST_EXPECT(Q<3>::count() == 0);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        BEAST_EXPECT(Q<2>::count() == 0);
        BEAST_EXPECT(Q<3>::count() == 0);
        // copy assign
        {
            variant<std::string> v1;
            BEAST_EXPECT(v1.index() == 0);
            variant<std::string> v2;
            v2 = v1;
            BEAST_EXPECT(v1.index() == 0);
            BEAST_EXPECT(v2.index() == 0);
            v2.emplace<1>("Hello");
            BEAST_EXPECT(v2.get<1>() == "Hello");
            variant<std::string> v3;
            v3 = v2;
            BEAST_EXPECT(v2.get<1>() == "Hello");
            BEAST_EXPECT(v3.get<1>() == "Hello");
        }
        {
            variant<Q<1>> v1;
            v1.emplace<1>();
            BEAST_EXPECT(! v1.get<1>().copy_assign);
            BEAST_EXPECT(Q<1>::count() == 1);
            variant<Q<1>> v2;
            v2.emplace<1>();
            BEAST_EXPECT(Q<1>::count() == 2);
            BEAST_EXPECT(! v2.get<1>().copy_assign);
            v2 = v1;
            BEAST_EXPECT(v1.index() == 1);
            BEAST_EXPECT(v2.index() == 1);
            BEAST_EXPECT(! v2.get<1>().copy_assign);
            BEAST_EXPECT(v2.get<1>().copy);
            BEAST_EXPECT(Q<1>::count() == 2);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        {
            variant<Q<1>, Q<2>, Q<3>> v1;
            v1.emplace<2>();
            BEAST_EXPECT(! v1.get<2>().copy_assign);
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 1);
            BEAST_EXPECT(Q<3>::count() == 0);
            variant<Q<1>, Q<2>, Q<3>> v2;
            v2.emplace<2>();
            BEAST_EXPECT(! v2.get<2>().copy_assign);
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 2);
            BEAST_EXPECT(Q<3>::count() == 0);
            v2 = v1;
            BEAST_EXPECT(v1.index() == 2);
            BEAST_EXPECT(v2.index() == 2);
            BEAST_EXPECT(! v2.get<2>().copy_assign);
            BEAST_EXPECT(v2.get<2>().copy);
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 2);
            BEAST_EXPECT(Q<3>::count() == 0);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        BEAST_EXPECT(Q<2>::count() == 0);
        BEAST_EXPECT(Q<3>::count() == 0);
        // get
        {
            variant<std::string> v;
            v.emplace<1>("Hello");
            v.get<1>() = "World!";
            BEAST_EXPECT(v.get<1>() == "World!");
        }
        // reset
        {
            variant<std::string> v;
            v.emplace<1>("Hello");
            v.reset();
            BEAST_EXPECT(v.index() == 0);
        }
        {
            variant<Q<1>> v;
            BEAST_EXPECT(Q<1>::count() == 0);
            v.emplace<1>();
            BEAST_EXPECT(Q<1>::count() == 1);
            v.reset();
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(v.index() == 0);
        }
        {
            variant<Q<1>, Q<2>, Q<3>> v;
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 0);
            BEAST_EXPECT(Q<3>::count() == 0);
            v.emplace<2>();
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 1);
            BEAST_EXPECT(Q<3>::count() == 0);
            v.reset();
            BEAST_EXPECT(Q<1>::count() == 0);
            BEAST_EXPECT(Q<2>::count() == 0);
            BEAST_EXPECT(Q<3>::count() == 0);
            BEAST_EXPECT(v.index() == 0);
        }
        BEAST_EXPECT(Q<1>::count() == 0);
        BEAST_EXPECT(Q<2>::count() == 0);
        BEAST_EXPECT(Q<3>::count() == 0);
    }

    void
    run()
    {
        testVariant();
    }
};

BOOST_BEAST_DEFINE_TESTSUITE(variant,core,beast);

} // detail
} // beast
} // boost
