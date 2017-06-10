//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_FIELDS_IPP
#define BEAST_HTTP_IMPL_FIELDS_IPP

#include <beast/core/buffer_cat.hpp>
#include <beast/core/static_string.hpp>
#include <beast/core/detail/ci_char_traits.hpp>
#include <beast/http/verb.hpp>
#include <beast/http/rfc7230.hpp>
#include <beast/http/status.hpp>
#include <beast/http/detail/chunk_encode.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>

namespace beast {
namespace http {

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

//------------------------------------------------------------------------------

template<class Allocator>
class basic_fields<Allocator>::reader
{
public:
    using iter_type = typename list_t::const_iterator;

    struct field_iterator
    {
        iter_type it_;

        using value_type = boost::asio::const_buffer;
        using pointer = value_type const*;
        using reference = value_type const;
        using difference_type = std::ptrdiff_t;
        using iterator_category =
            std::bidirectional_iterator_tag;

        field_iterator() = default;
        field_iterator(field_iterator&& other) = default;
        field_iterator(field_iterator const& other) = default;
        field_iterator& operator=(field_iterator&& other) = default;
        field_iterator& operator=(field_iterator const& other) = default;

        explicit
        field_iterator(iter_type it)
            : it_(it)
        {
        }

        bool
        operator==(field_iterator const& other) const
        {
            return it_ == other.it_;
        }

        bool
        operator!=(field_iterator const& other) const
        {
            return !(*this == other);
        }

        reference
        operator*() const
        {
            return it_->buffer();
        }

        field_iterator&
        operator++()
        {
            ++it_;
            return *this;
        }

        field_iterator
        operator++(int)
        {
            auto temp = *this;
            ++(*this);
            return temp;
        }

        field_iterator&
        operator--()
        {
            --it_;
            return *this;
        }

        field_iterator
        operator--(int)
        {
            auto temp = *this;
            --(*this);
            return temp;
        }
    };

    class field_range
    {
        field_iterator first_;
        field_iterator last_;

    public:
        using const_iterator =
            field_iterator;

        using value_type =
            typename const_iterator::value_type;

        field_range(field_range const&) = default;

        field_range(iter_type first, iter_type last)
            : first_(first)
            , last_(last)
        {
        }

        const_iterator
        begin() const
        {
            return first_;
        }

        const_iterator
        end() const
        {
            return last_;
        }
    };

    basic_fields const& f_;
    std::string s_;

public:
    using const_buffers_type =
        buffer_cat_view<
            boost::asio::const_buffers_1,
            field_range,
            boost::asio::const_buffers_1>;

    reader(basic_fields const& f, int version, verb v)
        : f_(f)
    {
        s_ = v == verb::unknown ?
            f_.get_method_impl().to_string() :
            to_string(v).to_string();
        s_ += " ";
        s_ += f_.get_target_impl().to_string();
        if(version == 11)
            s_ += " HTTP/1.1";
        else if(version == 10)
            s_ += " HTTP/1.0";
        else
            s_ += " HTTP/" +
                std::to_string(version / 10) + "." +
                std::to_string(version % 10);
        s_ += "\r\n";
    }

    reader(basic_fields const& f, int version, int code)
        : f_(f)
    {
        if(version == 11)
            s_ += "HTTP/1.1 ";
        else if(version == 10)
            s_ += "HTTP/1.0 ";
        else
            s_ += "HTTP/" +
                std::to_string(version / 10) + "." +
                std::to_string(version % 10) + " ";
        s_ += std::to_string(code) + " ";
        if(int_to_status(code) == status::unknown)
            s_ += f_.get_reason_impl().to_string();
        else
            s_ += obsolete_reason(int_to_status(code)).to_string();
        s_ += "\r\n";
    }

    const_buffers_type
    get() const
    {
        return buffer_cat(
            boost::asio::buffer(s_.data(), s_.size()),
            field_range(f_.list_.begin(), f_.list_.end()),
            detail::chunk_crlf());
    }
};

//------------------------------------------------------------------------------

template<class Allocator>
basic_fields<Allocator>::
element::
element(string_view name, string_view value)
    : off_(static_cast<off_t>(name.size() + 2))
    , len_(static_cast<off_t>(value.size()))
{
    char* p = reinterpret_cast<char*>(this + 1);
    data.first = p;
    data.off = off_;
    data.len = len_;

    p[off_-2] = ':';
    p[off_-1] = ' ';
    p[off_ + len_] = '\r';
    p[off_ + len_ + 1] = '\n';
    std::memcpy(p, name.data(), name.size());
    std::memcpy(p + off_, value.data(), value.size());
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
element::
name() const
{
    return {reinterpret_cast<
        char const*>(this + 1),
            static_cast<std::size_t>(off_ - 2)};
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
element::
value() const
{
    return {reinterpret_cast<
        char const*>(this + 1) + off_,
            static_cast<std::size_t>(len_)};
}

template<class Allocator>
inline
boost::asio::const_buffer
basic_fields<Allocator>::
element::
buffer() const
{
    return boost::asio::const_buffer{
        reinterpret_cast<char const*>(this + 1),
        static_cast<std::size_t>(off_) + len_ + 2};
}

//------------------------------------------------------------------------------

template<class Allocator>
basic_fields<Allocator>::
~basic_fields()
{
    delete_list();
    realloc_string(method_, {});
    realloc_string(target_or_reason_, {});
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(Allocator const& alloc)
    : alloc_(alloc)
{
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields&& other)
    : alloc_(std::move(other.alloc_))
    , set_(std::move(other.set_))
    , list_(std::move(other.list_))
    , method_(other.method_)
    , target_or_reason_(other.target_or_reason_)
{
    other.method_.clear();
    other.target_or_reason_.clear();
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields&& other, Allocator const& alloc)
    : alloc_(alloc)
{
    if(alloc_ != other.alloc_)
    {
        copy_all(other);
        other.clear_all();
    }
    else
    {
        set_ = std::move(other.set_);
        list_ = std::move(other.list_);
        method_ = other.method_;
        target_or_reason_ = other.target_or_reason_;
    }
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields const& other)
    : alloc_(alloc_traits::
        select_on_container_copy_construction(other.alloc_))
{
    copy_all(other);
}

template<class Allocator>
basic_fields<Allocator>::
basic_fields(basic_fields const& other,
        Allocator const& alloc)
    : alloc_(alloc)
{
    copy_all(other);
}

template<class Allocator>
template<class OtherAlloc>
basic_fields<Allocator>::
basic_fields(basic_fields<OtherAlloc> const& other)
{
    copy_all(other);
}

template<class Allocator>
template<class OtherAlloc>
basic_fields<Allocator>::
basic_fields(basic_fields<OtherAlloc> const& other,
        Allocator const& alloc)
    : alloc_(alloc)
{
    copy_all(other);
}

template<class Allocator>
auto
basic_fields<Allocator>::
operator=(basic_fields&& other) ->
    basic_fields&
{
    if(this == &other)
        return *this;
    move_assign(other, typename alloc_traits::
        propagate_on_container_move_assignment{});
    return *this;
}

template<class Allocator>
auto
basic_fields<Allocator>::
operator=(basic_fields const& other) ->
    basic_fields&
{
    copy_assign(other, typename alloc_traits::
        propagate_on_container_copy_assignment{});
    return *this;
}

template<class Allocator>
template<class OtherAlloc>
auto
basic_fields<Allocator>::
operator=(basic_fields<OtherAlloc> const& other) ->
    basic_fields&
{
    clear_all();
    copy_all(other);
    return *this;
}

//------------------------------------------------------------------------------

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
auto
basic_fields<Allocator>::
find(field name) const ->
    iterator
{
    auto const it = set_.find(
        to_string(name), less{});
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
string_view const
basic_fields<Allocator>::
operator[](field name) const
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
    delete_list();
    set_.clear();
    list_.clear();
}

template<class Allocator>
std::size_t
basic_fields<Allocator>::
erase(field f)
{
    return erase(to_string(f));
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
insert(field f, string_view value)
{
    insert(to_string(f), value);
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

template<class Allocator>
void
basic_fields<Allocator>::
replace(field name, string_view value)
{
    value = detail::trim(value);
    erase(name);
    insert(name, value);
}

//------------------------------------------------------------------------------

// Fields

template<class Allocator>
bool
basic_fields<Allocator>::
has_close_impl() const
{
    auto const fit = set_.find(
        to_string(field::connection), less{});
    if(fit == set_.end())
        return false;
    return token_list{fit->value()}.exists("close");
}

template<class Allocator>
bool
basic_fields<Allocator>::
has_chunked_impl() const
{
    auto const fit = set_.find(to_string(
        field::transfer_encoding), less{});
    if(fit == set_.end())
        return false;
    token_list const v{fit->value()};
    auto it = v.begin();
    if(it == v.end())
        return false;
    for(;;)
    {
        auto cur = it++;
        if(it == v.end())
            return beast::detail::ci_equal(
                *cur, "chunked");
    }
}

template<class Allocator>
bool
basic_fields<Allocator>::
has_content_length_impl() const
{
    auto const fit = set_.find(
        to_string(field::content_length), less{});
    return fit != set_.end();
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
set_method_impl(string_view s)
{
    realloc_string(method_, s);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
set_target_impl(string_view s)
{
    realloc_string(target_or_reason_, s);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
set_reason_impl(string_view s)
{
    realloc_string(target_or_reason_, s);
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
get_method_impl() const
{
    return method_;
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
get_target_impl() const
{
    return target_or_reason_;
}

template<class Allocator>
inline
string_view
basic_fields<Allocator>::
get_reason_impl() const
{
    return target_or_reason_;
}

//---

template<class Allocator>
inline
void
basic_fields<Allocator>::
content_length_impl(std::uint64_t n)
{
    this->erase("Content-Length");
    this->insert("Content-Length",
        to_static_string(n));
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
set_chunked_impl(bool v)
{
    BOOST_ASSERT(v);
    auto it = find("Transfer-Encoding");
    if(it == end())
        this->insert("Transfer-Encoding", "chunked");
    else
        this->replace("Transfer-Encoding",
            it->value().to_string() + ", chunked");
}

//------------------------------------------------------------------------------

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

template<class Allocator>
void
basic_fields<Allocator>::
realloc_string(string_view& dest, string_view s)
{
    s = detail::trim(s);
    if(dest.empty() && s.empty())
        return;
    auto a = typename std::allocator_traits<
        Allocator>::template rebind_alloc<
            char>(alloc_);
    if(! dest.empty())
    {
        a.deallocate(const_cast<char*>(
            dest.data()), dest.size());
        dest.clear();
    }
    if(! s.empty())
    {
        auto const p = a.allocate(s.size());
        std::memcpy(p, s.data(), s.size());
        dest = {p, s.size()};
    }
}

template<class Allocator>
template<class OtherAlloc>
void
basic_fields<Allocator>::
copy_all(basic_fields<OtherAlloc> const& other)
{
    for(auto const& e : other.list_)
        insert(e.name(), e.value());
    realloc_string(method_, other.method_);
    realloc_string(target_or_reason_,
        other.target_or_reason_);
}

template<class Allocator>
void
basic_fields<Allocator>::
clear_all()
{
    clear();
    realloc_string(method_, {});
    realloc_string(target_or_reason_, {});
}

template<class Allocator>
void
basic_fields<Allocator>::
delete_list()
{
    for(auto it = list_.begin(); it != list_.end();)
        delete_element(*it++);
}

//------------------------------------------------------------------------------

template<class Allocator>
inline
void
basic_fields<Allocator>::
move_assign(basic_fields& other, std::true_type)
{
    clear_all();
    set_ = std::move(other.set_);
    list_ = std::move(other.list_);
    method_ = other.method_;
    target_or_reason_ = other.target_or_reason_;
    other.method_.clear();
    other.target_or_reason_.clear();
    alloc_ = other.alloc_;
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
move_assign(basic_fields& other, std::false_type)
{
    clear_all();
    if(alloc_ != other.alloc_)
    {
        copy_all(other);
        other.clear_all();
    }
    else
    {
        set_ = std::move(other.set_);
        list_ = std::move(other.list_);
        method_ = other.method_;
        target_or_reason_ = other.target_or_reason_;
        other.method_.clear();
        other.target_or_reason_.clear();
    }
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
copy_assign(basic_fields const& other, std::true_type)
{
    clear_all();
    alloc_ = other.alloc_;
    copy_all(other);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
copy_assign(basic_fields const& other, std::false_type)
{
    clear_all();
    copy_all(other);
}

template<class Allocator>
void
swap(basic_fields<Allocator>& lhs,
    basic_fields<Allocator>& rhs)
{
    using alloc_traits = typename
        basic_fields<Allocator>::alloc_traits;
    lhs.swap(rhs, typename alloc_traits::
        propagate_on_container_swap{});
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
swap(basic_fields& other, std::true_type)
{
    using std::swap;
    swap(alloc_, other.alloc_);
    swap(set_, other.set_);
    swap(list_, other.list_);
    swap(method_, other.method_);
    swap(target_or_reason_, other.target_or_reason_);
}

template<class Allocator>
inline
void
basic_fields<Allocator>::
swap(basic_fields& other, std::false_type)
{
    BOOST_ASSERT(alloc_ == other.alloc_);
    using std::swap;
    swap(set_, other.set_);
    swap(list_, other.list_);
    swap(method_, other.method_);
    swap(target_or_reason_, other.target_or_reason_);
}

} // http
} // beast

#endif
