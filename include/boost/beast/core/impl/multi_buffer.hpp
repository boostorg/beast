//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_MULTI_BUFFER_HPP
#define BOOST_BEAST_IMPL_MULTI_BUFFER_HPP

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/config/workaround.hpp>
#include <boost/core/exchange.hpp>
#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <exception>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

/*  These diagrams illustrate the layout and state variables.

1   Input and output contained entirely in one element:

    0                            out_
    |<-------------+------------------------------------------->|
    in_pos_     out_pos_                                     out_end_


2   Output contained in first and second elements:

                    out_
    |<------+----------+------->|   |<----------+-------------->|
          in_pos_   out_pos_                 out_end_


3   Output contained in the second element:

                                                    out_
    |<------------+------------>|   |<----+-------------------->|
                in_pos_                out_pos_              out_end_


4   Output contained in second and third elements:

                                    out_
    |<-----+-------->|   |<-------+------>|   |<--------------->|
         in_pos_               out_pos_                      out_end_


5   Input sequence is empty:

                    out_
    |<------+------------------>|   |<-----------+------------->|
         out_pos_                             out_end_
          in_pos_


6   Output sequence is empty:

                                                    out_
    |<------+------------------>|   |<------+------------------>|
          in_pos_                        out_pos_
                                         out_end_


7   The end of output can point to the end of an element.
    But out_pos_ should never point to the end:

                                                    out_
    |<------+------------------>|   |<------+------------------>|
          in_pos_                        out_pos_            out_end_


8   When the input sequence entirely fills the last element and
    the output sequence is empty, out_ will point to the end of
    the list of buffers, and out_pos_ and out_end_ will be 0:


    |<------+------------------>|   out_     == list_.end()
          in_pos_                   out_pos_ == 0
                                    out_end_ == 0
*/
//------------------------------------------------------------------------------

template<class Allocator>
class basic_multi_buffer<Allocator>::element
    : public boost::intrusive::list_base_hook<
        boost::intrusive::link_mode<
            boost::intrusive::normal_link>>
{
    using size_type = typename
        detail::allocator_traits<Allocator>::size_type;

    size_type const size_;

public:
    element(element const&) = delete;

    explicit
    element(size_type n) noexcept
        : size_(n)
    {
    }

    size_type
    size() const noexcept
    {
        return size_;
    }

    char*
    data() const noexcept
    {
        return const_cast<char*>(
            reinterpret_cast<char const*>(this + 1));
    }
};

//------------------------------------------------------------------------------

#if BOOST_WORKAROUND(BOOST_MSVC, < 1910)
# pragma warning (push)
# pragma warning (disable: 4521) // multiple copy constructors specified
# pragma warning (disable: 4522) // multiple assignment operators specified
#endif

template<class Allocator>
template<bool IsMutable>
class basic_multi_buffer<Allocator>::readable_bytes
{
    basic_multi_buffer const* b_;

    friend class basic_multi_buffer;

    explicit
    readable_bytes(
        basic_multi_buffer const& b) noexcept
        : b_(&b)
    {
    }

public:
    using value_type = typename std::conditional<
        IsMutable,
        net::mutable_buffer,
        net::const_buffer>::type;

    class const_iterator;

    readable_bytes() = delete;
#if ! defined(_MSC_VER) || (_MSC_VER >= 1910)
    readable_bytes(readable_bytes const&) = default;
    readable_bytes& operator=(readable_bytes const&) = default;
#else
    readable_bytes(readable_bytes const& other)
        : b_(other.b_)
    {
    }

    readable_bytes& operator=(readable_bytes const& other)
    {
        b_ = other.b_;
        return *this;
    }
#endif

    template<
        bool IsMutable_ = IsMutable,
        class = typename std::enable_if<! IsMutable_>::type>
    readable_bytes(
        readable_bytes<true> const& other) noexcept
        : b_(other.b_)
    {
    }

    template<
        bool IsMutable_ = IsMutable,
        class = typename std::enable_if<! IsMutable_>::type>
    readable_bytes& operator=(
        readable_bytes<true> const& other) noexcept
    {
        b_ = other.b_;
        return *this;
    }

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    friend
    std::size_t
    buffer_size(readable_bytes const& buffers) noexcept
    {
        return buffers.b_->size();
    }
};

#if BOOST_WORKAROUND(BOOST_MSVC, < 1910)
# pragma warning (pop)
#endif

//------------------------------------------------------------------------------

template<class Allocator>
template<bool IsMutable>
class
    basic_multi_buffer<Allocator>::
    readable_bytes<IsMutable>::
    const_iterator
{
    basic_multi_buffer const* b_ = nullptr;
    typename list_type::const_iterator it_;

public:
    using value_type =
        typename readable_bytes::value_type;
    using pointer = value_type const*;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    const_iterator() = default;
    const_iterator(const_iterator&& other) = default;
    const_iterator(const_iterator const& other) = default;
    const_iterator& operator=(const_iterator&& other) = default;
    const_iterator& operator=(const_iterator const& other) = default;

    const_iterator(
        basic_multi_buffer const& b,
        typename list_type::const_iterator const& it) noexcept
        : b_(&b)
        , it_(it)
    {
    }

    bool
    operator==(const_iterator const& other) const noexcept
    {
        return b_ == other.b_ && it_ == other.it_;
    }

    bool
    operator!=(const_iterator const& other) const noexcept
    {
        return !(*this == other);
    }

    reference
    operator*() const noexcept
    {
        auto const& e = *it_;
        return value_type{e.data(),
           (b_->out_ == b_->list_.end() ||
                &e != &*b_->out_) ? e.size() : b_->out_pos_} +
                   (&e == &*b_->list_.begin() ? b_->in_pos_ : 0);
    }

    pointer
    operator->() const = delete;

    const_iterator&
    operator++() noexcept
    {
        ++it_;
        return *this;
    }

    const_iterator
    operator++(int) noexcept
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    const_iterator&
    operator--() noexcept
    {
        --it_;
        return *this;
    }

    const_iterator
    operator--(int) noexcept
    {
        auto temp = *this;
        --(*this);
        return temp;
    }
};

//------------------------------------------------------------------------------

template<class Allocator>
class basic_multi_buffer<Allocator>::mutable_buffers_type
{
    basic_multi_buffer const* b_;

    friend class basic_multi_buffer;

    explicit
    mutable_buffers_type(
        basic_multi_buffer const& b) noexcept
        : b_(&b)
    {
    }

public:
    using value_type = mutable_buffer;

    class const_iterator;

    mutable_buffers_type() = delete;
    mutable_buffers_type(mutable_buffers_type const&) = default;
    mutable_buffers_type& operator=(mutable_buffers_type const&) = default;

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
};

//------------------------------------------------------------------------------

template<class Allocator>
class basic_multi_buffer<Allocator>::mutable_buffers_type::const_iterator
{
    basic_multi_buffer const* b_ = nullptr;
    typename list_type::const_iterator it_;

public:
    using value_type =
        typename mutable_buffers_type::value_type;
    using pointer = value_type const*;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    const_iterator() = default;
    const_iterator(const_iterator&& other) = default;
    const_iterator(const_iterator const& other) = default;
    const_iterator& operator=(const_iterator&& other) = default;
    const_iterator& operator=(const_iterator const& other) = default;

    const_iterator(
        basic_multi_buffer const& b,
        typename list_type::const_iterator const& it) noexcept
        : b_(&b)
        , it_(it)
    {
    }

    bool
    operator==(const_iterator const& other) const noexcept
    {
        return b_ == other.b_ && it_ == other.it_;
    }

    bool
    operator!=(const_iterator const& other) const noexcept
    {
        return !(*this == other);
    }

    reference
    operator*() const noexcept
    {
        auto const& e = *it_;
        return value_type{e.data(),
            &e == &*std::prev(b_->list_.end()) ?
                b_->out_end_ : e.size()} +
                   (&e == &*b_->out_ ? b_->out_pos_ : 0);
    }

    pointer
    operator->() const = delete;

    const_iterator&
    operator++() noexcept
    {
        ++it_;
        return *this;
    }

    const_iterator
    operator++(int) noexcept
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    const_iterator&
    operator--() noexcept
    {
        --it_;
        return *this;
    }

    const_iterator
    operator--(int) noexcept
    {
        auto temp = *this;
        --(*this);
        return temp;
    }
};

//------------------------------------------------------------------------------

template<class Allocator>
template<bool IsMutable>
auto
basic_multi_buffer<Allocator>::
readable_bytes<IsMutable>::
begin() const noexcept ->
    const_iterator
{
    return const_iterator{*b_, b_->list_.begin()};
}

template<class Allocator>
template<bool IsMutable>
auto
basic_multi_buffer<Allocator>::
readable_bytes<IsMutable>::
end() const noexcept ->
    const_iterator
{
    return const_iterator{*b_, b_->out_ ==
        b_->list_.end() ? b_->list_.end() :
            std::next(b_->out_)};
}

template<class Allocator>
auto
basic_multi_buffer<Allocator>::
mutable_buffers_type::
begin() const noexcept ->
    const_iterator
{
    return const_iterator{*b_, b_->out_};
}

template<class Allocator>
auto
basic_multi_buffer<Allocator>::
mutable_buffers_type::
end() const noexcept ->
    const_iterator
{
    return const_iterator{*b_, b_->list_.end()};
}

//------------------------------------------------------------------------------

template<class Allocator>
basic_multi_buffer<Allocator>::
~basic_multi_buffer()
{
    delete_list();
}

template<class Allocator>
basic_multi_buffer<Allocator>::
basic_multi_buffer(
    Allocator const& alloc) noexcept
    : boost::empty_value<base_alloc_type>(
        boost::empty_init_t(), alloc)
    , max_(alloc_traits::max_size(this->get()))
    , out_(list_.end())
{
}

template<class Allocator>
basic_multi_buffer<Allocator>::
basic_multi_buffer(
    std::size_t limit,
    Allocator const& alloc) noexcept
    : boost::empty_value<
        base_alloc_type>(boost::empty_init_t(), alloc)
    , max_(limit)
    , out_(list_.end())
{
}

template<class Allocator>
basic_multi_buffer<Allocator>::
basic_multi_buffer(
    basic_multi_buffer&& other) noexcept
    : boost::empty_value<base_alloc_type>(
        boost::empty_init_t(), std::move(other.get()))
    , max_(other.max_)
    , in_size_(boost::exchange(other.in_size_, 0))
    , in_pos_(boost::exchange(other.in_pos_, 0))
    , out_pos_(boost::exchange(other.out_pos_, 0))
    , out_end_(boost::exchange(other.out_end_, 0))
{
    auto const at_end =
        other.out_ == other.list_.end();
    list_ = std::move(other.list_);
    out_ = at_end ? list_.end() : other.out_;
    other.out_ = other.list_.end();
}

template<class Allocator>
basic_multi_buffer<Allocator>::
basic_multi_buffer(
    basic_multi_buffer&& other,
    Allocator const& alloc) 
    : boost::empty_value<
        base_alloc_type>(boost::empty_init_t(), alloc)
    , max_(other.max_)
{
    if(this->get() != other.get())
    {
        out_ = list_.end();
        copy_from(other);
        other.clear();
    }
    else
    {
        auto const at_end =
            other.out_ == other.list_.end();
        list_ = std::move(other.list_);
        out_ = at_end ? list_.end() : other.out_;
        in_size_ = other.in_size_;
        in_pos_ = other.in_pos_;
        out_pos_ = other.out_pos_;
        out_end_ = other.out_end_;
        other.in_size_ = 0;
        other.out_ = other.list_.end();
        other.in_pos_ = 0;
        other.out_pos_ = 0;
        other.out_end_ = 0;
    }
}

template<class Allocator>
basic_multi_buffer<Allocator>::
basic_multi_buffer(
    basic_multi_buffer const& other)
    : boost::empty_value<base_alloc_type>(
        boost::empty_init_t(), alloc_traits::
            select_on_container_copy_construction(
                other.get()))
    , max_(other.max_)
    , out_(list_.end())
{
    copy_from(other);
}

template<class Allocator>
basic_multi_buffer<Allocator>::
basic_multi_buffer(
    basic_multi_buffer const& other,
    Allocator const& alloc)
    : boost::empty_value<base_alloc_type>(
        boost::empty_init_t(), alloc)
    , max_(other.max_)
    , out_(list_.end())
{
    copy_from(other);
}

template<class Allocator>
template<class OtherAlloc>
basic_multi_buffer<Allocator>::
basic_multi_buffer(
        basic_multi_buffer<OtherAlloc> const& other)
    : out_(list_.end())
{
    copy_from(other);
}

template<class Allocator>
template<class OtherAlloc>
basic_multi_buffer<Allocator>::
basic_multi_buffer(
    basic_multi_buffer<OtherAlloc> const& other,
        allocator_type const& alloc)
    : boost::empty_value<
        base_alloc_type>(boost::empty_init_t(), alloc)
    , max_(other.max_)
    , out_(list_.end())
{
    copy_from(other);
}

template<class Allocator>
auto
basic_multi_buffer<Allocator>::
operator=(basic_multi_buffer&& other) ->
    basic_multi_buffer&
{
    if(this == &other)
        return *this;
    clear();
    max_ = other.max_;
    move_assign(other, pocma{});
    return *this;
}

template<class Allocator>
auto
basic_multi_buffer<Allocator>::
operator=(basic_multi_buffer const& other) ->
basic_multi_buffer&
{
    if(this == &other)
        return *this;
    copy_assign(other, pocca{});
    return *this;
}

template<class Allocator>
template<class OtherAlloc>
auto
basic_multi_buffer<Allocator>::
operator=(
    basic_multi_buffer<OtherAlloc> const& other) ->
        basic_multi_buffer&
{
    clear();
    copy_from(other);
    return *this;
}

//------------------------------------------------------------------------------

template<class Allocator>
std::size_t
basic_multi_buffer<Allocator>::
capacity() const noexcept
{
    auto pos = out_;
    if(pos == list_.end())
        return in_size_;
    auto n = pos->size() - out_pos_;
    while(++pos != list_.end())
        n += pos->size();
    return in_size_ + n;
}

template<class Allocator>
auto
basic_multi_buffer<Allocator>::
data() const noexcept ->
    const_buffers_type
{
    return const_buffers_type(*this);
}

template<class Allocator>
auto
basic_multi_buffer<Allocator>::
data() noexcept ->
    mutable_data_type
{
    return mutable_data_type(*this);
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
reserve(std::size_t n)
{
    if(n > alloc_traits::max_size(this->get()))
        BOOST_THROW_EXCEPTION(std::length_error(
            "A basic_multi_buffer exceeded the allocator's maximum size"));
    std::size_t total = in_size_;
    if(n <= total)
        return;
    if(out_ != list_.end())
    {
        total += out_->size() - out_pos_;
        if(n <= total)
            return;
        auto it = out_;
        while(++it != list_.end())
        {
            total += it->size();
            if(n <= total)
                return;
        }
    }
    BOOST_ASSERT(n > total);
    (void)prepare(n - total);
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
shrink_to_fit() noexcept
{
    if( out_ != list_.end() &&
        out_ != list_.iterator_to(list_.back()))
    {
        list_type extra;
        extra.splice(extra.end(), list_,
            std::next(out_), list_.end());
        for(auto it = extra.begin(); it != extra.end();)
        {
            auto& e = *it++;
            auto const len = sizeof(e) + e.size();
            e.~element();
            alloc_traits::deallocate(this->get(),
                reinterpret_cast<char*>(&e), len);
        }
    }
}

template<class Allocator>
auto
basic_multi_buffer<Allocator>::
prepare(size_type n) ->
    mutable_buffers_type
{
    if(in_size_ + n > max_)
        BOOST_THROW_EXCEPTION(std::length_error{
            "A basic_multi_buffer exceeded its maximum size"});
    list_type reuse;
    std::size_t total = in_size_;
    // put all empty buffers on reuse list
    if(out_ != list_.end())
    {
        total += out_->size() - out_pos_;
        if(out_ != list_.iterator_to(list_.back()))
        {
            out_end_ = out_->size();
            reuse.splice(reuse.end(), list_,
                std::next(out_), list_.end());
        #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
            debug_check();
        #endif
        }
        auto const avail = out_->size() - out_pos_;
        if(n > avail)
        {
            out_end_ = out_->size();
            n -= avail;
        }
        else
        {
            out_end_ = out_pos_ + n;
            n = 0;
        }
    #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
        debug_check();
    #endif
    }
    // get space from reuse buffers
    while(n > 0 && ! reuse.empty())
    {
        auto& e = reuse.front();
        reuse.erase(reuse.iterator_to(e));
        list_.push_back(e);
        total += e.size();
        if(n > e.size())
        {
            out_end_ = e.size();
            n -= e.size();
        }
        else
        {
            out_end_ = n;
            n = 0;
        }
    #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
        debug_check();
    #endif
    }
    BOOST_ASSERT(total <= max_);
    if(! reuse.empty() || n > 0)
    {
        for(auto it = reuse.begin(); it != reuse.end();)
        {
            auto& e = *it++;
            reuse.erase(list_.iterator_to(e));
            auto const len = sizeof(e) + e.size();
            e.~element();
            alloc_traits::deallocate(this->get(),
                reinterpret_cast<char*>(&e), len);
        }
        if(n > 0)
        {
            static auto const growth_factor = 2.0f;
            auto const size =
                (std::min<std::size_t>)(
                    max_ - total,
                    (std::max<std::size_t>)({
                        static_cast<std::size_t>(
                            in_size_ * growth_factor - in_size_),
                        512,
                        n}));
            auto &e = *::new(alloc(
                sizeof(element) + size)) element(size);
            list_.push_back(e);
            if(out_ == list_.end())
                out_ = list_.iterator_to(e);
            out_end_ = n;
        #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
            debug_check();
        #endif
        }
    }
    return mutable_buffers_type(*this);
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
commit(size_type n) noexcept
{
    if(list_.empty())
        return;
    if(out_ == list_.end())
        return;
    auto const back =
        list_.iterator_to(list_.back());
    while(out_ != back)
    {
        auto const avail =
            out_->size() - out_pos_;
        if(n < avail)
        {
            out_pos_ += n;
            in_size_ += n;
        #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
            debug_check();
        #endif
            return;
        }
        ++out_;
        n -= avail;
        out_pos_ = 0;
        in_size_ += avail;
    #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
        debug_check();
    #endif
    }

    n = (std::min)(n, out_end_ - out_pos_);
    out_pos_ += n;
    in_size_ += n;
    if(out_pos_ == out_->size())
    {
        ++out_;
        out_pos_ = 0;
        out_end_ = 0;
    }
#if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
    debug_check();
#endif
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
consume(size_type n) noexcept
{
    if(list_.empty())
        return;
    for(;;)
    {
        if(list_.begin() != out_)
        {
            auto const avail =
                list_.front().size() - in_pos_;
            if(n < avail)
            {
                in_size_ -= n;
                in_pos_ += n;
            #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
                debug_check();
            #endif
                break;
            }
            n -= avail;
            in_size_ -= avail;
            in_pos_ = 0;
            auto& e = list_.front();
            list_.erase(list_.iterator_to(e));
            auto const len = sizeof(e) + e.size();
            e.~element();
            alloc_traits::deallocate(this->get(),
                reinterpret_cast<char*>(&e), len);
        #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
            debug_check();
        #endif
        }
        else
        {
            auto const avail = out_pos_ - in_pos_;
            if(n < avail)
            {
                in_size_ -= n;
                in_pos_ += n;
            }
            else
            {
                in_size_ = 0;
                if(out_ != list_.iterator_to(list_.back()) ||
                    out_pos_ != out_end_)
                {
                    in_pos_ = out_pos_;
                }
                else
                {
                    // Input and output sequences are empty, reuse buffer.
                    // Alternatively we could deallocate it.
                    in_pos_ = 0;
                    out_pos_ = 0;
                    out_end_ = 0;
                }
            }
        #if BOOST_BEAST_MULTI_BUFFER_DEBUG_CHECK
            debug_check();
        #endif
            break;
        }
    }
}

template<class Allocator>
template<class OtherAlloc>
void
basic_multi_buffer<Allocator>::
copy_from(basic_multi_buffer<OtherAlloc> const& other)
{
    clear();
    max_ = other.max_;
    if(other.size() == 0)
        return;
    commit(net::buffer_copy(
        prepare(other.size()), other.data()));
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
move_assign(basic_multi_buffer& other, std::false_type)
{
    if(this->get() != other.get())
    {
        copy_from(other);
        other.clear();
    }
    else
    {
        move_assign(other, std::true_type{});
    }
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
move_assign(basic_multi_buffer& other, std::true_type) noexcept
{
    this->get() = std::move(other.get());
    auto const at_end =
        other.out_ == other.list_.end();
    list_ = std::move(other.list_);
    out_ = at_end ? list_.end() : other.out_;

    in_size_ = other.in_size_;
    in_pos_ = other.in_pos_;
    out_pos_ = other.out_pos_;
    out_end_ = other.out_end_;
    max_ = other.max_;

    other.in_size_ = 0;
    other.out_ = other.list_.end();
    other.in_pos_ = 0;
    other.out_pos_ = 0;
    other.out_end_ = 0;
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
copy_assign(
    basic_multi_buffer const& other, std::false_type)
{
    copy_from(other);
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
copy_assign(
    basic_multi_buffer const& other, std::true_type)
{
    clear();
    this->get() = other.get();
    copy_from(other);
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
swap(basic_multi_buffer& other) noexcept
{
    swap(other, typename
        alloc_traits::propagate_on_container_swap{});
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
swap(basic_multi_buffer& other, std::true_type) noexcept
{
    using std::swap;
    auto const at_end0 =
        out_ == list_.end();
    auto const at_end1 =
        other.out_ == other.list_.end();
    swap(this->get(), other.get());
    swap(list_, other.list_);
    swap(out_, other.out_);
    if(at_end1)
        out_ = list_.end();
    if(at_end0)
        other.out_ = other.list_.end();
    swap(in_size_, other.in_size_);
    swap(in_pos_, other.in_pos_);
    swap(out_pos_, other.out_pos_);
    swap(out_end_, other.out_end_);
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
swap(basic_multi_buffer& other, std::false_type) noexcept
{
    BOOST_ASSERT(this->get() == other.get());
    using std::swap;
    auto const at_end0 =
        out_ == list_.end();
    auto const at_end1 =
        other.out_ == other.list_.end();
    swap(list_, other.list_);
    swap(out_, other.out_);
    if(at_end1)
        out_ = list_.end();
    if(at_end0)
        other.out_ = other.list_.end();
    swap(in_size_, other.in_size_);
    swap(in_pos_, other.in_pos_);
    swap(out_pos_, other.out_pos_);
    swap(out_end_, other.out_end_);
}

template<class Allocator>
void
swap(
    basic_multi_buffer<Allocator>& lhs,
    basic_multi_buffer<Allocator>& rhs) noexcept
{
    lhs.swap(rhs);
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
delete_list() noexcept
{
    for(auto it = list_.begin(); it != list_.end();)
    {
        auto& e = *it++;
        auto const len = sizeof(e) + e.size();
        e.~element();
        alloc_traits::deallocate(this->get(),
            reinterpret_cast<char*>(&e), len);
    }
}

template<class Allocator>
char*
basic_multi_buffer<Allocator>::
alloc(std::size_t n)
{
    if(n > alloc_traits::max_size(this->get()))
        BOOST_THROW_EXCEPTION(std::length_error(
            "A basic_multi_buffer exceeded the allocator's maximum size"));
    return alloc_traits::allocate(this->get(), n);
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
clear() noexcept
{
    delete_list();
    list_.clear();
    out_ = list_.end();
    in_size_ = 0;
    in_pos_ = 0;
    out_pos_ = 0;
    out_end_ = 0;
}

template<class Allocator>
void
basic_multi_buffer<Allocator>::
debug_check() const
{
#ifndef NDEBUG
    using net::buffer_size;
    BOOST_ASSERT(buffer_size(data()) == in_size_);
    if(list_.empty())
    {
        BOOST_ASSERT(in_pos_ == 0);
        BOOST_ASSERT(in_size_ == 0);
        BOOST_ASSERT(out_pos_ == 0);
        BOOST_ASSERT(out_end_ == 0);
        BOOST_ASSERT(out_ == list_.end());
        return;
    }

    auto const& front = list_.front();

    BOOST_ASSERT(in_pos_ < front.size());

    if(out_ == list_.end())
    {
        BOOST_ASSERT(out_pos_ == 0);
        BOOST_ASSERT(out_end_ == 0);
    }
    else
    {
        auto const& out = *out_;
        auto const& back = list_.back();

        BOOST_ASSERT(out_end_ <= back.size());
        BOOST_ASSERT(out_pos_ <  out.size());
        BOOST_ASSERT(&out != &front || out_pos_ >= in_pos_);
        BOOST_ASSERT(&out != &front || out_pos_ - in_pos_ == in_size_);
        BOOST_ASSERT(&out != &back  || out_pos_ <= out_end_);
    }
#endif
}

} // beast
} // boost

#endif
