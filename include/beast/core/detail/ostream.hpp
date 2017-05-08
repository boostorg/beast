//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_DETAIL_OSTREAM_HPP
#define BEAST_DETAIL_OSTREAM_HPP

#include <boost/asio/buffer.hpp>
#include <boost/utility/base_from_member.hpp>
#include <memory>
#include <ostream>
#include <streambuf>
#include <type_traits>
#include <utility>

namespace beast {
namespace detail {

template<class Buffers>
class buffers_helper
{
    Buffers b_;

public:
    explicit
    buffers_helper(Buffers const& b)
        : b_(b)
    {
    }

    template<class B>
    friend
    std::ostream&
    operator<<(std::ostream& os,
        buffers_helper<B> const& v);
};

template<class Buffers>
std::ostream&
operator<<(std::ostream& os,
    buffers_helper<Buffers> const& v)
{
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    for(auto const& b : v.b_)
        os.write(buffer_cast<char const*>(b),
            buffer_size(b));
    return os;
}

//------------------------------------------------------------------------------

template<
    class DynamicBuffer, class CharT, class Traits>
class ostream_buffer
    : public std::basic_streambuf<CharT, Traits>
{
    using int_type = typename
        std::basic_streambuf<CharT, Traits>::int_type;

    using traits_type = typename
        std::basic_streambuf<CharT, Traits>::traits_type;

    static std::size_t constexpr max_size = 512;

    DynamicBuffer& buf_;

public:
    ostream_buffer(ostream_buffer&&) = default;
    ostream_buffer(ostream_buffer const&) = delete;

    ~ostream_buffer() noexcept;

    explicit
    ostream_buffer(DynamicBuffer& buf);

    int_type
    overflow(int_type ch) override;

    int
    sync() override;

private:
    void
    prepare();

    void
    flush(int extra = 0);
};

template<class DynamicBuffer, class CharT, class Traits>
ostream_buffer<DynamicBuffer, CharT, Traits>::
~ostream_buffer() noexcept
{
    sync();
}

template<class DynamicBuffer, class CharT, class Traits>
ostream_buffer<DynamicBuffer, CharT, Traits>::
ostream_buffer(DynamicBuffer& buf)
    : buf_(buf)
{
    prepare();
}

template<class DynamicBuffer, class CharT, class Traits>
auto
ostream_buffer<DynamicBuffer, CharT, Traits>::
overflow(int_type ch) ->
    int_type
{
    if(! Traits::eq_int_type(ch, Traits::eof()))
    {
        Traits::assign(*this->pptr(), ch);
        flush(1);
        prepare();
        return ch;
    }
    flush();
    return traits_type::eof();
}

template<class DynamicBuffer, class CharT, class Traits>
int
ostream_buffer<DynamicBuffer, CharT, Traits>::
sync()
{
    flush();
    prepare();
    return 0;
}

template<class DynamicBuffer, class CharT, class Traits>
void
ostream_buffer<DynamicBuffer, CharT, Traits>::
prepare()
{
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    auto mbs = buf_.prepare(
        read_size_helper(buf_, max_size));
    auto const mb = *mbs.begin();
    auto const p = buffer_cast<CharT*>(mb);
    this->setp(p,
        p + buffer_size(mb) / sizeof(CharT) - 1);
}

template<class DynamicBuffer, class CharT, class Traits>
void
ostream_buffer<DynamicBuffer, CharT, Traits>::
flush(int extra)
{
    buf_.commit(
        (this->pptr() - this->pbase() + extra) *
            sizeof(CharT));
}

//------------------------------------------------------------------------------

template<class DynamicBuffer,
    class CharT, class Traits, bool isMovable>
class ostream_helper;

template<class DynamicBuffer, class CharT, class Traits>
class ostream_helper<
        DynamicBuffer, CharT, Traits, true>
    : public std::basic_ostream<CharT, Traits>
{
    ostream_buffer<DynamicBuffer, CharT, Traits> osb_;

public:
    explicit
    ostream_helper(DynamicBuffer& buf);

    ostream_helper(ostream_helper&& other);
};

template<class DynamicBuffer, class CharT, class Traits>
ostream_helper<DynamicBuffer, CharT, Traits, true>::
ostream_helper(DynamicBuffer& buf)
    : std::basic_ostream<CharT, Traits>(
        &this->osb_)
    , osb_(buf)
{
}

template<class DynamicBuffer, class CharT, class Traits>
ostream_helper<DynamicBuffer, CharT, Traits, true>::
ostream_helper(
        ostream_helper&& other)
    : std::basic_ostream<CharT, Traits>(&osb_)
    , osb_(std::move(other.osb_))
{
}

// This work-around is for libstdc++ versions that
// don't have a movable std::basic_streambuf

template<class DynamicBuffer, class CharT, class Traits>
struct ostream_helper<
        DynamicBuffer, CharT, Traits, false>
    : private boost::base_from_member<
        std::unique_ptr<ostream_buffer<
            DynamicBuffer, CharT, Traits>>>
    , std::basic_ostream<CharT, Traits>
{
    explicit
    ostream_helper(DynamicBuffer& buf);

    ostream_helper(ostream_helper&& other);
};

template<class DynamicBuffer, class CharT, class Traits>
ostream_helper<DynamicBuffer, CharT, Traits, false>::
ostream_helper(DynamicBuffer& buf)
    : boost::base_from_member<std::unique_ptr<
        ostream_buffer<DynamicBuffer, CharT, Traits>>>(
            new ostream_buffer<DynamicBuffer, CharT, Traits>(buf))
    , std::basic_ostream<CharT, Traits>(
        this->member.get())
{
}

template<class DynamicBuffer, class CharT, class Traits>
ostream_helper<DynamicBuffer, CharT, Traits, false>::
ostream_helper(ostream_helper&& other)
    : boost::base_from_member<std::unique_ptr<
        ostream_buffer<DynamicBuffer, CharT, Traits>>>(
            std::move(other.member))
    , std::basic_ostream<CharT, Traits>(
        this->member.get())
{
}

} // detail
} // beast

#endif
