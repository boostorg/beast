//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_QUERY_HPP
#define SOCKS_QUERY_HPP

#include <socks/config.hpp>
#include <iterator>
#include <utility>

namespace socks {

/** A ForwardRange representing a reference to a parsed query string.
*/
class query
{
public:
    explicit
    query(string_view s)
        : s_(s)
    {
    }

    class const_iterator
    {
    public:
        using value_type = std::pair<string_view, string_view>;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using pointer = const value_type*;
        using iterator_category = std::forward_iterator_tag;

        const_iterator() = default;

        explicit
        const_iterator(string_view s)
            : s_(s)
        {
            parse();
        }

        ~const_iterator() = default;

        reference
        operator*() const noexcept
        {
            return const_cast<reference>(value_);
        }

        pointer
        operator->() const noexcept
        {
            return &value_;
        }

        const_iterator&
        operator++() noexcept
        {
            increment();
            return *this;
        }

        const_iterator
        operator++(int) noexcept
        {
            const_iterator tmp = *this;
            increment();
            return tmp;
        }

        bool
        operator==(const const_iterator &other) const noexcept
        {
            if ((value_.first.data() == other.value_.first.data() &&
                value_.first.size() == other.value_.first.size()) &&
                (value_.second.data() == other.value_.second.data() &&
                value_.second.size() == other.value_.second.size()))
                return true;
            return false;
        }

        bool
        operator!=(const const_iterator &other) const noexcept
        {
            return !(*this == other);
        }

        string_view
        key() const noexcept
        {
            return value_.first;
        }

        string_view
        value() const noexcept
        {
            return value_.second;
        }

    private:
        BOOST_BEAST_DECL
        void
        parse() noexcept;

        BOOST_BEAST_DECL
        void
        increment();

        string_view s_;
        value_type value_;
    };

    BOOST_BEAST_DECL
    const_iterator
    begin() const
    {
        return const_iterator{s_};
    }

    BOOST_BEAST_DECL
    const_iterator
    end() const
    {
        return {};
    }

private:
    string_view s_;
};

} // socks

#include <socks/impl/query.ipp>

#endif
