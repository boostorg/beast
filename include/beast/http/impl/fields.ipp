//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_FIELDS_IPP
#define BEAST_HTTP_IMPL_FIELDS_IPP

#include <beast/http/detail/rfc7230.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>

namespace beast {
namespace http {

//------------------------------------------------------------------------------

template<class Allocator>
basic_fields<Allocator>::
~basic_fields()
{
    delete_all();
}

//------------------------------------------------------------------------------

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
method_impl() const
{
    return (*this)[":method"];
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
target_impl() const
{
    return (*this)[":target"];
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
reason_impl() const
{
    return (*this)[":reason"];
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
method_impl(string_view s)
{
    if(s.empty())
        this->erase(":method");
    else
        this->replace(":method", s);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
target_impl(string_view s)
{
    if(s.empty())
        this->erase(":target");
    else
        return this->replace(":target", s);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
reason_impl(string_view s)
{
    if(s.empty())
        this->erase(":reason");
    else
        this->replace(":reason", s);
}

//------------------------------------------------------------------------------

template<class Allocator>
class basic_fields<Allocator>::
    const_iterator
{
    using iter_type = typename list_t::const_iterator;

    iter_type it_;

    template<class Alloc>
    friend class beast::http::basic_fields;

    const_iterator(iter_type it)
        : it_(it)
    {
    }

public:
    using value_type = typename
        basic_fields<Allocator>::value_type;
    using pointer = value_type const*;
    using reference = value_type const&;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    const_iterator() = default;
    const_iterator(const_iterator&& other) = default;
    const_iterator(const_iterator const& other) = default;
    const_iterator& operator=(const_iterator&& other) = default;
    const_iterator& operator=(const_iterator const& other) = default;

    bool
    operator==(const_iterator const& other) const
    {
        return it_ == other.it_;
    }

    bool
    operator!=(const_iterator const& other) const
    {
        return !(*this == other);
    }

    reference
    operator*() const
    {
        return it_->data;
    }

    pointer
    operator->() const
    {
        return &**this;
    }

    const_iterator&
    operator++()
    {
        ++it_;
        return *this;
    }

    const_iterator
    operator++(int)
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    const_iterator&
    operator--()
    {
        --it_;
        return *this;
    }

    const_iterator
    operator--(int)
    {
        auto temp = *this;
        --(*this);
        return temp;
    }
};

template<class Allocator>
void
basic_fields<Allocator>::
delete_all()
{
    for(auto it = list_.begin(); it != list_.end();)
        delete_element(*it++);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
move_assign(basic_fields& other, std::false_type)
{
    if(alloc_ != other.alloc_)
    {
        copy_from(other);
        other.clear();
    }
    else
    {
        set_ = std::move(other.set_);
        list_ = std::move(other.list_);
    }
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
move_assign(basic_fields& other, std::true_type)
{
    alloc_ = std::move(other.alloc_);
    set_ = std::move(other.set_);
    list_ = std::move(other.list_);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
copy_assign(basic_fields const& other, std::false_type)
{
    copy_from(other);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
copy_assign(basic_fields const& other, std::true_type)
{
    alloc_ = other.alloc_;
    copy_from(other);
}

template<class Allocator>
auto
basic_fields<Allocator>::
new_element(string_view name, string_view value) ->
    element&
{
    if(name.size() + 2 >
            (std::numeric_limits<off_t>::max)())
        BOOST_THROW_EXCEPTION(std::length_error{
            "field name too large"});
    if(value.size() + 2 >
            (std::numeric_limits<off_t>::max)())
        BOOST_THROW_EXCEPTION(std::length_error{
            "field value too large"});
    value = detail::trim(value);
    std::uint16_t const off =
        static_cast<off_t>(name.size() + 2);
    std::uint16_t const len =
        static_cast<off_t>(value.size());
    auto const p = alloc_traits::allocate(alloc_,
        1 + (off + len + 2 + sizeof(element) - 1) /
            sizeof(element));
    alloc_traits::construct(alloc_, p, name, value);
    return *p;
}

template<class Allocator>
void
basic_fields<Allocator>::
delete_element(element& e)
{
    auto const n = 1 +
        (e.data.off + e.data.len + 2 +
            sizeof(element) - 1) / sizeof(element);
    alloc_traits::destroy(alloc_, &e);
    alloc_traits::deallocate(alloc_, &e, n);
}

//------------------------------------------------------------------------------

template<class Allocator>
basic_fields<Allocator>::
basic_fields(Allocator const& alloc)
    : alloc_(alloc)
{
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields&& other)
    : set_(std::move(other.set_))
    , list_(std::move(other.list_))
    , alloc_(std::move(other.alloc_))
{
}

template<class Allocator>
auto
basic_fields<Allocator>::
operator=(basic_fields&& other) ->
    basic_fields&
{
    if(this == &other)
        return *this;
    clear();
    move_assign(other, std::integral_constant<bool,
        alloc_traits::propagate_on_container_move_assignment::value>{});
    return *this;
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields const& other)
    : basic_fields(alloc_traits::
        select_on_container_copy_construction(other.alloc_))
{
    copy_from(other);
}

template<class Allocator>
auto
basic_fields<Allocator>::
operator=(basic_fields const& other) ->
    basic_fields&
{
    clear();
    copy_assign(other, std::integral_constant<bool,
        alloc_traits::propagate_on_container_copy_assignment::value>{});
    return *this;
}

template<class Allocator>
template<class OtherAlloc>
basic_fields<Allocator>::
basic_fields(basic_fields<OtherAlloc> const& other)
{
    copy_from(other);
}

template<class Allocator>
template<class OtherAlloc>
auto
basic_fields<Allocator>::
operator=(basic_fields<OtherAlloc> const& other) ->
    basic_fields&
{
    clear();
    copy_from(other);
    return *this;
}

template<class Allocator>
std::size_t
basic_fields<Allocator>::
count(string_view name) const
{
    auto const it = set_.find(name, less{});
    if(it == set_.end())
        return 0;
    auto const last = set_.upper_bound(name, less{});
    return static_cast<std::size_t>(std::distance(it, last));
}

template<class Allocator>
auto
basic_fields<Allocator>::
find(string_view name) const ->
    iterator
{
    auto const it = set_.find(name, less{});
    if(it == set_.end())
        return list_.end();
    return list_.iterator_to(*it);
}

template<class Allocator>
string_view const
basic_fields<Allocator>::
operator[](string_view name) const
{
    auto const it = find(name);
    if(it == end())
        return {};
    return it->value();
}

template<class Allocator>
void
basic_fields<Allocator>::
clear() noexcept
{
    delete_all();
    list_.clear();
    set_.clear();
}

template<class Allocator>
std::size_t
basic_fields<Allocator>::
erase(string_view name)
{
    auto it = set_.find(name, less{});
    if(it == set_.end())
        return 0;
    auto const last = set_.upper_bound(name, less{});
    std::size_t n = 1;
    for(;;)
    {
        auto& e = *it++;
        set_.erase(set_.iterator_to(e));
        list_.erase(list_.iterator_to(e));
        delete_element(e);
        if(it == last)
            break;
        ++n;
    }
    return n;
}

template<class Allocator>
void
basic_fields<Allocator>::
insert(string_view name, string_view value)
{
    auto& e = new_element(name, value);
    set_.insert_before(set_.upper_bound(name, less{}), e);
    list_.push_back(e);
}

template<class Allocator>
void
basic_fields<Allocator>::
replace(string_view name, string_view value)
{
    value = detail::trim(value);
    erase(name);
    insert(name, value);
}

} // http
} // beast

#endif
