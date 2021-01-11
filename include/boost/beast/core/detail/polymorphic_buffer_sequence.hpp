//
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_POLYMORPHIC_BUFFER_SEQUENCE_HPP
#define BOOST_BEAST_DETAIL_POLYMORPHIC_BUFFER_SEQUENCE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <boost/core/exchange.hpp>
#include <algorithm>
#include <iterator>

namespace boost {
namespace beast {
namespace detail {

template<class T>
struct polymorphic_buffer_sequence_rule;

template<>
struct polymorphic_buffer_sequence_rule<net::const_buffer>
{
    template<class T>
    struct check : net::is_const_buffer_sequence<T> {};

    using value_type = net::const_buffer;
};

template<>
struct polymorphic_buffer_sequence_rule<net::mutable_buffer>
{
    template<class T>
    struct check : net::is_mutable_buffer_sequence<T> {};

    using value_type = net::mutable_buffer;
};

template<class, class=void>
struct is_forward_iterator : std::false_type {};
template<class Iter>
struct is_forward_iterator<
    Iter, typename std::enable_if<
    std::is_base_of<
        std::forward_iterator_tag, typename
        std::iterator_traits<Iter>::iterator_category>::value
    >::type> : std::true_type {};

template<class T>
class basic_polymorphic_buffer_sequence
    : private polymorphic_buffer_sequence_rule<T>
{
    using typename polymorphic_buffer_sequence_rule<T>::value_type;
    using polymorphic_buffer_sequence_rule<T>::check;

    enum { capacity_value = 15 };

    union
    {
        value_type static_[capacity_value];
        std::vector<value_type> dynamic_;
    };

    std::size_t size_ = 0;

public:

    using iterator = value_type*;

    using const_iterator = value_type const*;

    static constexpr
    std::size_t
    static_capacity()
    {
        return capacity_value;
    }

    static constexpr
    bool
    is_dynamic(std::size_t required)
    {
        return required > static_capacity();
    }

    template<class BufferSequence,
        typename std::enable_if<
            polymorphic_buffer_sequence_rule<T>::template
                check<BufferSequence>::value>::type* = nullptr,
        typename std::enable_if<
            !std::is_base_of<basic_polymorphic_buffer_sequence<T>, typename
                std::decay<BufferSequence>::type>::value>::type* = nullptr
            >
    basic_polymorphic_buffer_sequence(BufferSequence const& sequence);

    basic_polymorphic_buffer_sequence() noexcept;

    basic_polymorphic_buffer_sequence(value_type v1, value_type v2) noexcept;

    template<
        class Iter,
        typename std::enable_if<
            is_forward_iterator<Iter>::value>::type* = nullptr>
    basic_polymorphic_buffer_sequence(Iter first , Iter last);

    basic_polymorphic_buffer_sequence(
        basic_polymorphic_buffer_sequence&& other) noexcept;

    basic_polymorphic_buffer_sequence(
        basic_polymorphic_buffer_sequence const& other);

    basic_polymorphic_buffer_sequence&
    operator=(basic_polymorphic_buffer_sequence&& other) noexcept;

    basic_polymorphic_buffer_sequence&
    operator=(basic_polymorphic_buffer_sequence const& other);

    ~basic_polymorphic_buffer_sequence();

    template<
        class Iter,
        typename std::enable_if<
            is_forward_iterator<Iter>::value
            >::type* = nullptr>
    basic_polymorphic_buffer_sequence&
    append(Iter first , Iter last);

    template<class BufferSequence,
        typename std::enable_if<
            polymorphic_buffer_sequence_rule<T>::template
                check<BufferSequence>::value>::type* = nullptr
        >
    basic_polymorphic_buffer_sequence&
    append(BufferSequence const& sequence);

    BOOST_BEAST_DECL
    basic_polymorphic_buffer_sequence&
    clear();

    BOOST_BEAST_DECL
    const_iterator
    begin() const;

    BOOST_BEAST_DECL
    iterator
    begin();

    BOOST_BEAST_DECL
    const_iterator
    end() const;

    BOOST_BEAST_DECL
    iterator
    end();

    BOOST_BEAST_DECL
    void
    consume(std::size_t n);

    BOOST_BEAST_DECL
    BOOST_ATTRIBUTE_NODISCARD
    auto
    prefix_copy(std::size_t n) const &
        -> basic_polymorphic_buffer_sequence;

    BOOST_BEAST_DECL
    BOOST_ATTRIBUTE_NODISCARD
    auto
    prefix_copy(std::size_t n) &&
        -> basic_polymorphic_buffer_sequence&&;

    BOOST_BEAST_DECL
    auto
    prefix(std::size_t n)
    -> basic_polymorphic_buffer_sequence&;

    BOOST_BEAST_DECL
    void shrink(std::size_t n);

    BOOST_BEAST_DECL
    std::size_t
    size() const
    {
        if (is_dynamic(size_))
            return dynamic_.size();
        else
            return size_;
    }

    // mutates the polymorphic buffer
    BOOST_BEAST_DECL
    void
    push_front(value_type buf) &;

    BOOST_BEAST_DECL
    auto
    append(value_type buf) & -> basic_polymorphic_buffer_sequence&;

    // copies the buffer sequence

    BOOST_BEAST_DECL
    auto
    push_front_copy(value_type buf) const &
    -> basic_polymorphic_buffer_sequence;

    BOOST_BEAST_DECL
    auto
    push_front_copy(value_type buf) &&
    -> basic_polymorphic_buffer_sequence&&;

    BOOST_BEAST_DECL
    auto
    operator+(value_type r) const &
    -> basic_polymorphic_buffer_sequence;

    BOOST_BEAST_DECL
    auto
    operator+(value_type r) &&
    -> basic_polymorphic_buffer_sequence&&;

    BOOST_BEAST_DECL
    auto
    front() const
    -> value_type;
};

using polymorphic_const_buffer_sequence =
    basic_polymorphic_buffer_sequence<net::const_buffer>;

using polymorphic_mutable_buffer_sequence =
    basic_polymorphic_buffer_sequence<net::mutable_buffer>;

template<class T>
template<
    class BufferSequence, typename std::enable_if<
        polymorphic_buffer_sequence_rule<T>::template
            check<BufferSequence>::value>::type*,
    typename std::enable_if<
        !std::is_base_of<basic_polymorphic_buffer_sequence<T>, typename
            std::decay<BufferSequence>::type>::value>::type*
>
basic_polymorphic_buffer_sequence<T>::
    basic_polymorphic_buffer_sequence(const BufferSequence &sequence)
    : size_(std::distance(boost::asio::buffer_sequence_begin(sequence),
                        boost::asio::buffer_sequence_end(sequence)))
{
    if (is_dynamic(size_))
    {
        new (&dynamic_) std::vector<value_type> (
            boost::asio::buffer_sequence_begin(sequence),
            boost::asio::buffer_sequence_end(sequence));
    }
    else
    {
        std::copy(boost::asio::buffer_sequence_begin(sequence),
            boost::asio::buffer_sequence_end(sequence),
            static_);
    }
}

template<class T>
template<
    class BufferSequence,
    typename std::enable_if<
        polymorphic_buffer_sequence_rule<T>::template check<BufferSequence>::value>::type*>
basic_polymorphic_buffer_sequence<T> &
basic_polymorphic_buffer_sequence<T>::append(const BufferSequence &sequence)
{
    return append(net::buffer_sequence_begin(sequence),
                  net::buffer_sequence_end(sequence));
}

template<class T>
template<
    class Iter, typename std::enable_if<
    is_forward_iterator<Iter>::value>::type*>
basic_polymorphic_buffer_sequence<T>::
basic_polymorphic_buffer_sequence(Iter first , Iter last)
: size_(std::min(std::size_t(std::distance(first, last)),
                 static_capacity() + 1))
{
    if (is_dynamic(size_))
        new (&dynamic_) std::vector<value_type>(first, last);
    else
        std::copy(first, last, static_);
}

template<class T>
template<
    class Iter,
    typename std::enable_if<
        is_forward_iterator<Iter>::value
        >::type*>
basic_polymorphic_buffer_sequence<T>&
basic_polymorphic_buffer_sequence<T>::append(Iter first , Iter last)
{
    while(first != last)
        append(*first++);
    return *this;
}

} // detail
} // beast
} // boost

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/core/detail/polymorphic_buffer_sequence.ipp>
#endif

#endif

