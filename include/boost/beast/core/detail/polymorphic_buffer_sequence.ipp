#ifndef BOOST_BEAST_CORE_DETAIL_POLYMORPHIC_BUFFER_SEQUENCE_HPP
#define BOOST_BEAST_CORE_DETAIL_POLYMORPHIC_BUFFER_SEQUENCE_HPP

#include <boost/beast/core/detail/polymorphic_buffer_sequence.hpp>

namespace boost {
namespace beast {
namespace detail {

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::begin() const
    -> const_iterator
{
    if (is_dynamic(size_))
        return dynamic_.data();
    else
        return static_;
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::begin()
    -> iterator
{
    if (is_dynamic(size_))
        return dynamic_.data();
    else
        return static_;
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::end() const
    -> const_iterator
{
    if (is_dynamic(size_))
        return dynamic_.data() + dynamic_.size();
    else
        return static_ + size_;
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::end()
    -> iterator
{
    if (is_dynamic(size_))
        return dynamic_.data() + dynamic_.size();
    else
        return static_ + size_;
}

template<class T>
void
basic_polymorphic_buffer_sequence<T>::consume(std::size_t n)
{
    if (is_dynamic(size_))
    {
        while(n && dynamic_.size())
        {
            auto& front = dynamic_.front();
            auto m = std::min(n, front.size());
            if (m < front.size())
            {
                front += m;
            }
            else
            {
                dynamic_.erase(dynamic_.begin());
            }
            n -= m;
        }
    }
    else
    {
        while(n && size_)
        {
            auto& front = static_[0];
            auto m = std::min(n, front.size());
            if (m < front.size())
            {
                front += m;
            }
            else
            {
                std::copy(static_ + 1, static_ + size_, static_);
                size_ -= 1;
            }
            n -= m;
        }
    }
}

template<class T>
void
basic_polymorphic_buffer_sequence<T>::
push_front(value_type buf) &
{
    if(is_dynamic(size_))
    {
        dynamic_.insert(dynamic_.begin(), buf);
    }
    else if (is_dynamic(size_ + 1))
    {
        auto dyn = std::vector<value_type>(size_ + 1);
        dyn[0] = buf;
        std::copy(static_, static_ + size_, &dyn[1]);
        size_ += 1;
        new (&dynamic_) std::vector<value_type>(std::move(dyn));
    }
    else
    {
        std::copy_backward(static_,
            static_ + size_,
            static_ + size_ + 1);
        size_ += 1;
        static_[0] = buf;
    }
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::
push_front_copy(value_type buf) const&
    -> basic_polymorphic_buffer_sequence
{
    auto result = basic_polymorphic_buffer_sequence();

    if (is_dynamic(size_ + 1))
    {
        new (&result.dynamic_) std::vector<value_type> (size_ + 1);
        // defer the size until after new in case of exceptions
        result.size_ = size_ + 1;
        result.dynamic_[0] = buf;
        std::copy(begin(), end(), result.dynamic_.begin() + 1);
    }
    else
    {
        result.size_ = size_ + 1;
        result.static_[0] = buf;
        std::copy(begin(), end(), &result.static_[1]);
    }

    return result;
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::
push_front_copy(value_type buf) &&
    -> basic_polymorphic_buffer_sequence&&
{
    push_front(buf);
    return std::move(*this);
}


template<class T>
auto
basic_polymorphic_buffer_sequence<T>::
prefix_copy(std::size_t n) const &
    -> basic_polymorphic_buffer_sequence
{
    std::size_t tot = 0;
    auto first = begin();
    auto last =
        std::find_if(first, end(), [&tot, &n](value_type const& buf)
        {
            if (tot >= n)
                return true;

            tot += buf.size();
            return false;
        });
    auto result = basic_polymorphic_buffer_sequence(begin(), last);
    result.shrink(tot - std::min(n, tot));
    return result;
}

// specialisation where *this is an rvalue - mutate in-place
// and return an rvalue to self
template<class T>
auto
basic_polymorphic_buffer_sequence<T>::
prefix_copy(std::size_t n) &&
    -> basic_polymorphic_buffer_sequence&&
{
    return std::move(this->prefix(n));
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::
prefix(std::size_t n)
-> basic_polymorphic_buffer_sequence&
{
    std::size_t tot = 0;
    auto pred = [&tot, &n](value_type const& buf)
    {
        if (tot >= n)
            return true;

        tot += buf.size();
        return false;
    };

    if (is_dynamic(size_))
    {
        auto last =
            std::find_if(dynamic_.begin(),
                         dynamic_.end(),
                         pred);
        dynamic_.erase(last, dynamic_.end());
    }
    else
    {
        auto last =
            std::find_if(static_,
                         static_ + size_,
                         pred);
        size_ -= std::distance(last, static_ + size_);
    }

    shrink(tot - std::min(n, tot));
    return *this;
}

template<class T>
void
basic_polymorphic_buffer_sequence<T>::
shrink(std::size_t n)
{
    auto first = begin();
    auto last = end();
    while (first != last && n)
    {
        --last;
        auto x = std::min(n, last->size());
        n -= x;
        *last = value_type(last->data(), last->size() - x);
        if(last->size() == 0)
        {
            if(is_dynamic(size_))
            {
                dynamic_.pop_back();
            }
            else
            {
                size_ -= 1;
            }
        }
    }
}

template<class T>
basic_polymorphic_buffer_sequence<T>::basic_polymorphic_buffer_sequence() noexcept
: size_(0)
{
}

template<class T>
basic_polymorphic_buffer_sequence<T>::~basic_polymorphic_buffer_sequence()
{
    if (is_dynamic(size_))
        dynamic_.~vector();
}

template<class T>
basic_polymorphic_buffer_sequence<T>::
basic_polymorphic_buffer_sequence(value_type v1 , value_type v2) noexcept
: size_(2)
{
    BOOST_ASSERT(!is_dynamic(size_));
    static_[0] = v1;
    static_[1] = v2;
}

template<class T>
basic_polymorphic_buffer_sequence<T>::
    basic_polymorphic_buffer_sequence(
        const basic_polymorphic_buffer_sequence &other)
: size_(0)
{
    if (is_dynamic(other.size_))
    {
        new (&dynamic_) std::vector<value_type>(other.dynamic_);
    }
    else
    {
        std::copy(other.begin(), other.end(), static_);
    }

    // size deferred until after the vector copy, which could throw
    size_ = other.size_;
}

template<class T>
basic_polymorphic_buffer_sequence<T>::
    basic_polymorphic_buffer_sequence(
        basic_polymorphic_buffer_sequence &&other) noexcept
: size_(boost::exchange(other.size_, 0))
{
    if (is_dynamic(size_))
    {
        new (&dynamic_) std::vector<value_type>(std::move(other.dynamic_));
        other.dynamic_.~vector();
    }
    else
    {
        std::copy(other.static_, other.static_ + size_, static_);
    }
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::
    operator=(
        const basic_polymorphic_buffer_sequence &other)
    -> basic_polymorphic_buffer_sequence&
{
    if(is_dynamic(size_))
    {
        if(is_dynamic(other.size_))
        {
            dynamic_.assign(other.dynamic_.begin(), other.dynamic_.end());
        }
        else
        {
            dynamic_.assign(other.static_, other.static_ + other.size_);
        }
    }
    else
    {
        if (is_dynamic(other.size_))
        {
            new (&dynamic_) std::vector<value_type>(other.dynamic_.begin(), other.dynamic_.end());
            size_ = other.size_;
        }
        else
        {
            std::copy(other.static_, other.static_ + other.size_, static_);
            size_ = other.size_;
        }
    }
    return *this;
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::
    operator=(
        basic_polymorphic_buffer_sequence &&other) noexcept
    -> basic_polymorphic_buffer_sequence&
{
    if(is_dynamic(size_))
    {
        if(is_dynamic(other.size_))
        {
            dynamic_ = std::move(other.dynamic_);
        }
        else
        {
            dynamic_.assign(other.static_, other.static_ + other.size_);
        }
    }
    else
    {
        if (is_dynamic(other.size_))
        {
            new (&dynamic_) std::vector<value_type>(std::move(other.dynamic_));
            size_ = other.size_;
        }
        else
        {
            std::copy(other.static_, other.static_ + other.size_, static_);
            size_ = other.size_;
        }
    }
    return *this;
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::
append(value_type buf) &
-> basic_polymorphic_buffer_sequence&
{
    if (is_dynamic(size_))
    {
#ifdef BOOST_BEAST_POLYMORPHIC_BUFFER_METRICS
        assert(dynamic_.capacity() > dynamic_.size());
#endif
        dynamic_.push_back(buf);
    }
    else
    {
        if (is_dynamic(size_ + 1))
        {
            std::vector<value_type> vec;
            vec.reserve(size_ * 2);
            vec.assign(static_, static_ + size_);
            vec.push_back(buf);
            new (&dynamic_) std::vector<value_type>(std::move(vec));
            size_ += 1;
        }
        else
        {
            static_[size_] = buf;
            size_ += 1;
        }
    }
    return *this;
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::operator+(value_type r) const &
-> basic_polymorphic_buffer_sequence
{
    auto l = *this;
    l.append(r);
    return l;
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::operator+(value_type r) &&
-> basic_polymorphic_buffer_sequence&&
{
    append(r);
    return std::move(*this);
}

template<class T>
auto
basic_polymorphic_buffer_sequence<T>::front() const
-> value_type
{
    if (size_ == 0)
    {
        return value_type {};
    }
    else if (is_dynamic(size_))
    {
        if (dynamic_.begin() == dynamic_.end())
            return value_type {};
        return dynamic_.front();
    }
    else
    {
        return static_[0];
    }
}

template<class T>
basic_polymorphic_buffer_sequence<T>&
basic_polymorphic_buffer_sequence<T>::clear()
{
    if (is_dynamic(size_))
        dynamic_.clear();
    else
        size_ = 0;
    return *this;
}

#ifndef BOOST_BEAST_HEADER_ONLY

template class basic_polymorphic_buffer_sequence<net::mutable_buffer>;
template class basic_polymorphic_buffer_sequence<net::const_buffer>;

#endif // BOOST_BEAST_HEADER_ONLY
} // detail
} // beast
} // boost

#endif // BOOST_BEAST_CORE_DETAIL_POLYMORPHIC_BUFFER_SEQUENCE_HPP
