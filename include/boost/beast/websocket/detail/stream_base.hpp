//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_STREAM_BASE_HPP
#define BOOST_BEAST_WEBSOCKET_STREAM_BASE_HPP

#include <boost/beast/websocket/option.hpp>
#include <boost/beast/websocket/detail/pmd_extension.hpp>
#include <boost/beast/zlib/deflate_stream.hpp>
#include <boost/beast/zlib/inflate_stream.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/buffer.hpp>
#include <cstdint>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// used to order reads and writes
class soft_mutex
{
    int id_ = 0;

public:
    soft_mutex() = default;
    soft_mutex(soft_mutex const&) = delete;
    soft_mutex& operator=(soft_mutex const&) = delete;

    soft_mutex(soft_mutex&& other) noexcept
        : id_(other.id_)
    {
        other.id_ = 0;
    }

    soft_mutex& operator=(soft_mutex&& other) noexcept
    {
        id_ = other.id_;
        other.id_ = 0;
        return *this;
    }

    // VFALCO I'm not too happy that this function is needed
    void reset()
    {
        id_ = 0;
    }

    bool is_locked() const
    {
        return id_ != 0;
    }

    template<class T>
    bool is_locked(T const*) const
    {
        return id_ == T::id;
    }

    template<class T>
    void lock(T const*)
    {
        BOOST_ASSERT(id_ == 0);
        id_ = T::id;
    }

    template<class T>
    void unlock(T const*)
    {
        BOOST_ASSERT(id_ == T::id);
        id_ = 0;
    }

    template<class T>
    bool try_lock(T const*)
    {
        // If this assert goes off it means you are attempting to
        // simultaneously initiate more than one of same asynchronous
        // operation, which is not allowed. For example, you must wait
        // for an async_read to complete before performing another
        // async_read.
        //
        BOOST_ASSERT(id_ != T::id);
        if(id_ != 0)
            return false;
        id_ = T::id;
        return true;
    }

    template<class T>
    bool try_unlock(T const*)
    {
        if(id_ != T::id)
            return false;
        id_ = 0;
        return true;
    }
};

template<bool deflateSupported>
struct stream_base
{
    // State information for the permessage-deflate extension
    struct pmd_type
    {
        // `true` if current read message is compressed
        bool rd_set = false;

        zlib::deflate_stream zo;
        zlib::inflate_stream zi;
    };

    std::unique_ptr<pmd_type>   pmd_;           // pmd settings or nullptr
    permessage_deflate          pmd_opts_;      // local pmd options
    detail::pmd_offer           pmd_config_;    // offer (client) or negotiation (server)

    // return `true` if current message is deflated
    bool
    rd_deflated() const
    {
        return pmd_ && pmd_->rd_set;
    }

    // set whether current message is deflated
    // returns `false` on protocol violation
    bool
    rd_deflated(bool rsv1)
    {
        if(pmd_)
        {
            pmd_->rd_set = rsv1;
            return true;
        }
        return ! rsv1; // pmd not negotiated
    }

    template<class ConstBufferSequence>
    bool
    deflate(
        boost::asio::mutable_buffer& out,
        buffers_suffix<ConstBufferSequence>& cb,
        bool fin,
        std::size_t& total_in,
        error_code& ec);

    void
    do_context_takeover_write(role_type role);

    void
    inflate(
        zlib::z_params& zs,
        zlib::Flush flush,
        error_code& ec);

    void
    do_context_takeover_read(role_type role);
};

template<>
struct stream_base<false>
{
    // These stubs are for avoiding linking in the zlib
    // code when permessage-deflate is not enabled.

    bool
    rd_deflated() const
    {
        return false;
    }

    bool
    rd_deflated(bool rsv1)
    {
        return ! rsv1;
    }

    template<class ConstBufferSequence>
    bool
    deflate(
        boost::asio::mutable_buffer&,
        buffers_suffix<ConstBufferSequence>&,
        bool,
        std::size_t&,
        error_code&)
    {
        return false;
    }

    void
    do_context_takeover_write(role_type)
    {
    }

    void
    inflate(
        zlib::z_params&,
        zlib::Flush,
        error_code&)
    {
    }

    void
    do_context_takeover_read(role_type)
    {
    }
};

} // detail
} // websocket
} // beast
} // boost

#endif
