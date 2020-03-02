//
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TESTING_INTERVALS_HPP
#define BOOST_BEAST_TESTING_INTERVALS_HPP

#include <cstddef>

namespace boost {
namespace beast {
namespace testing {

struct intervals
{
    struct iterator
    {
        using value_type = std::size_t;
        using pointer = value_type *;
        using reference = value_type &;
        using different_type = std::size_t;
        using iterator_category = std::input_iterator_tag;

        iterator()
            : i_(0), n_(0), limit_(0)
        {}

        iterator(std::size_t i, std::size_t n, std::size_t limit)
            : i_(i), n_(n), limit_(limit)
        {}

        bool operator==(iterator const& b) const
        {
            auto* ref = b.limit_ == 0 ? this : &b;
            return i_ == ref->limit_;
        }

        bool operator!=(iterator const& b) const
        {
            return !((*this) == b);
        }

        iterator& operator++()
        {
            i_ += (std::min)(n_, limit_ - i_);
            return *this;
        }

        iterator operator++(int)
        {
            auto result = *this;
            ++(*this);
            return result;
        }

        value_type operator*() const
        {
            return i_;
        }

    private:
        std::size_t i_;
        std::size_t n_;
        std::size_t limit_;
    };

    intervals(std::size_t start, std::size_t limit, std::size_t interval)
        : start_(start)
        , limit_(limit)
        , interval_(interval)
    {}

    auto begin() const -> iterator
    {
        return iterator(start_, interval_, limit_);
    }

    auto end() const -> iterator
    {
        return iterator();
    }
private:
    std::size_t start_;
    std::size_t limit_;
    std::size_t interval_;
};

}
}
}

#endif
