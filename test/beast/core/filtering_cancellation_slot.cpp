//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


// Test that header file is self-contained.
#include <boost/beast/core/detail/filtering_cancellation_slot.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {


struct filtering_cancellation_slot_test : beast::unit_test::suite
{
    void
    run()
    {
        using ct = net::cancellation_type;
        ct fired = ct::none;
        auto l = [&fired](ct tp){fired = tp;};

        net::cancellation_signal sl;

        detail::filtering_cancellation_slot<> slot{ct::terminal, sl.slot()};
        slot.type |= ct::total;
        slot = sl.slot();

        slot.assign(l);

        BEAST_EXPECT(fired == ct::none);
        sl.emit(ct::total);
        BEAST_EXPECT(fired == ct::total);
        sl.emit(ct::partial);
        BEAST_EXPECT(fired == ct::total);
        sl.emit(ct::terminal);
        BEAST_EXPECT(fired == ct::terminal);
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,filtering_cancellation_slot);

} // beast
} // boost