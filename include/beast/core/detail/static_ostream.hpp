//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_DETAIL_STATIC_OSTREAM_HPP
#define BEAST_DETAIL_STATIC_OSTREAM_HPP

#include <locale>
#include <ostream>
#include <streambuf>

namespace beast {
namespace detail {

// http://www.mr-edd.co.uk/blog/beginners_guide_streambuf

class static_ostream_buffer
    : public std::basic_streambuf<char>
{
    using CharT = char;
    using Traits = std::char_traits<CharT>;
    using int_type = typename
        std::basic_streambuf<CharT, Traits>::int_type;
    using traits_type = typename
        std::basic_streambuf<CharT, Traits>::traits_type;

    char buf_[128];
    std::string s_;
    std::size_t len_ = 0;

public:
    static_ostream_buffer(static_ostream_buffer&&) = delete;
    static_ostream_buffer(static_ostream_buffer const&) = delete;

    static_ostream_buffer()
    {
        this->setp(buf_, buf_ + sizeof(buf_) - 1);
    }

    ~static_ostream_buffer() noexcept
    {
    }

    string_view
    str() const
    {
        if(! s_.empty())
            return {s_.data(), len_};
        return {buf_, len_};
    }

    int_type
    overflow(int_type ch) override
    {
        if(! Traits::eq_int_type(ch, Traits::eof()))
        {
            Traits::assign(*this->pptr(),
                static_cast<CharT>(ch));
            flush(1);
            prepare();
            return ch;
        }
        flush();
        return traits_type::eof();
    }

    int
    sync() override
    {
        flush();
        prepare();
        return 0;
    }

private:
    void
    prepare()
    {
        static auto const growth_factor = 1.5;

        if(len_ < sizeof(buf_) - 1)
        {
            this->setp(&buf_[len_],
                &buf_[sizeof(buf_) - 2]);
            return;
        }
        if(s_.empty())
        {
            s_.resize(static_cast<std::size_t>(
                growth_factor * len_));
            Traits::copy(&s_[0], buf_, len_);
        }
        else
        {
            s_.resize(static_cast<std::size_t>(
                growth_factor * len_));
        }
        this->setp(&s_[len_], &s_[len_] +
            s_.size() - len_ - 1);
    }

    void
    flush(int extra = 0)
    {
        len_ += static_cast<std::size_t>(
            this->pptr() - this->pbase() + extra);
    }
};

class static_ostream : public std::basic_ostream<char>
{
    static_ostream_buffer osb_;

public:
    static_ostream()
        : std::basic_ostream<char>(&this->osb_)
    {
        imbue(std::locale::classic());
    }

    string_view
    str() const
    {
        return osb_.str();
    }
};

} // detail
} // beast

#endif
