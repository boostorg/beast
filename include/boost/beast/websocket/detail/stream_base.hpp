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

#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/option.hpp>
#include <boost/beast/websocket/detail/pmd_extension.hpp>
#include <boost/beast/zlib/deflate_stream.hpp>
#include <boost/beast/zlib/inflate_stream.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/detail/chacha.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/align/aligned_alloc.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/core/exchange.hpp>
#include <atomic>
#include <cstdint>
#include <memory>
#include <new>
#include <random>

// Turn this on to avoid using thread_local
//#define BOOST_BEAST_NO_THREAD_LOCAL 1

#ifdef BOOST_BEAST_NO_THREAD_LOCAL
#include <atomic>
#include <mutex>
#endif

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
        : id_(boost::exchange(other.id_, 0))
    {
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

//------------------------------------------------------------------------------

struct stream_prng
{
    bool secure_prng_ = true;

    struct prng_type
    {
        std::minstd_rand fast;
        beast::detail::chacha<20> secure;

#if BOOST_BEAST_NO_THREAD_LOCAL
        prng_type* next = nullptr;
#endif

        prng_type(std::uint32_t const* v, std::uint64_t stream)
            : fast(static_cast<typename decltype(fast)::result_type>(
                v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6] + v[7] + stream))
            , secure(v, stream)
        {
        }
    };

    class prng_ref
    {
        prng_type* p_;

    public:
        prng_ref& operator=(prng_ref&&) = delete;

        explicit
        prng_ref(prng_type& p)
            : p_(&p)
        {
        }

        prng_ref(prng_ref&& other)
            : p_(boost::exchange(other.p_, nullptr))
        {
        }

#ifdef BOOST_BEAST_NO_THREAD_LOCAL
        ~prng_ref()
        {
            if(p_)
                pool::impl().release(*p_);
        }
#endif

        prng_type*
        operator->() const
        {
            return p_;
        }
    };

#ifndef BOOST_BEAST_NO_THREAD_LOCAL
    static
    prng_ref
    prng()
    {
        static std::atomic<std::uint64_t> stream{0};
        thread_local prng_type p{seed(), stream++};
        return prng_ref(p);
    }

#else
    static
    prng_ref
    prng()
    {
        return prng_ref(pool::impl().acquire());
    }

#endif

    static
    std::uint32_t const*
    seed(std::seed_seq* ss = nullptr)
    {
        static seed_data d(ss);
        return d.v;
    }

    std::uint32_t
    create_mask()
    {
        auto p = prng();
        if(secure_prng_)
            for(;;)
                if(auto key = p->secure())
                    return key;
        for(;;)
            if(auto key = p->fast())
                return key;
    }

private:
    struct seed_data
    {
        std::uint32_t v[8];

        explicit
        seed_data(std::seed_seq* pss)
        {
            if(! pss)
            {
                std::random_device g;
                std::seed_seq ss{
                    g(), g(), g(), g(), g(), g(), g(), g()};
                ss.generate(v, v+8);
            }
            else
            {
                pss->generate(v, v+8);
            }
        }
    };

#ifdef BOOST_BEAST_NO_THREAD_LOCAL
    class pool
    {
        prng_type* head_ = nullptr;
        std::atomic<std::uint64_t> n_{0};
        std::mutex m_;

    public:
        ~pool()
        {
            for(auto p = head_; p;)
            {
                auto next = p->next;
                p->~prng_type();
                boost::alignment::aligned_free(p);
                p = next;
            }
        }

        prng_type&
        acquire()
        {
            for(;;)
            {
                std::lock_guard<std::mutex> lock(m_);
                if(! head_)
                    break;
                auto p = head_;
                head_ = head_->next;
                return *p;
            }
            auto p = boost::alignment::aligned_alloc(
                16, sizeof(prng_type));
            if(! p)
                BOOST_THROW_EXCEPTION(std::bad_alloc{});
            return *(::new(p) prng_type(seed(), n_++));
        }

        void
        release(prng_type& p)
        {
            std::lock_guard<std::mutex> lock(m_);
            p.next = head_;
            head_ = &p;
        }

        static
        pool&
        impl()
        {
            static pool instance;
            return instance;
        }
    };
#endif
};

//------------------------------------------------------------------------------

template<bool deflateSupported>
struct stream_base : stream_prng
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
        net::mutable_buffer& out,
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

    template<class Body, class Allocator>
    void
    build_response_pmd(
        http::response<http::string_body>& res,
        http::request<Body,
            http::basic_fields<Allocator>> const& req)
    {
        detail::pmd_offer offer;
        detail::pmd_offer unused;
        detail::pmd_read(offer, req);
        detail::pmd_negotiate(res, unused, offer, pmd_opts_);
    }

    void
    on_response_pmd(http::response<http::string_body> const& res)
    {
        detail::pmd_offer offer;
        detail::pmd_read(offer, res);
        // VFALCO see if offer satisfies pmd_config_,
        //        return an error if not.
        pmd_config_ = offer; // overwrite for now
    }

    template<class Allocator>
    void
    do_pmd_config(
        http::basic_fields<Allocator> const& h)
    {
        detail::pmd_read(pmd_config_, h);
    }

    void
    set_option_pmd(permessage_deflate const& o)
    {
        if( o.server_max_window_bits > 15 ||
            o.server_max_window_bits < 9)
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "invalid server_max_window_bits"});
        if( o.client_max_window_bits > 15 ||
            o.client_max_window_bits < 9)
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "invalid client_max_window_bits"});
        if( o.compLevel < 0 ||
            o.compLevel > 9)
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "invalid compLevel"});
        if( o.memLevel < 1 ||
            o.memLevel > 9)
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "invalid memLevel"});
        pmd_opts_ = o;
    }

    void
    get_option_pmd(permessage_deflate& o)
    {
        o = pmd_opts_;
    }


    void
    build_request_pmd(http::request<http::empty_body>& req)
    {
        if(pmd_opts_.client_enable)
        {
            detail::pmd_offer config;
            config.accept = true;
            config.server_max_window_bits =
                pmd_opts_.server_max_window_bits;
            config.client_max_window_bits =
                pmd_opts_.client_max_window_bits;
            config.server_no_context_takeover =
                pmd_opts_.server_no_context_takeover;
            config.client_no_context_takeover =
                pmd_opts_.client_no_context_takeover;
            detail::pmd_write(req, config);
        }
    }

    void
    open_pmd(role_type role)
    {
        if(((role == role_type::client &&
                pmd_opts_.client_enable) ||
            (role == role_type::server &&
                pmd_opts_.server_enable)) &&
            pmd_config_.accept)
        {
            detail::pmd_normalize(pmd_config_);
            pmd_.reset(new typename
                detail::stream_base<deflateSupported>::pmd_type);
            if(role == role_type::client)
            {
                pmd_->zi.reset(
                    pmd_config_.server_max_window_bits);
                pmd_->zo.reset(
                    pmd_opts_.compLevel,
                    pmd_config_.client_max_window_bits,
                    pmd_opts_.memLevel,
                    zlib::Strategy::normal);
            }
            else
            {
                pmd_->zi.reset(
                    pmd_config_.client_max_window_bits);
                pmd_->zo.reset(
                    pmd_opts_.compLevel,
                    pmd_config_.server_max_window_bits,
                    pmd_opts_.memLevel,
                    zlib::Strategy::normal);
            }
        }
    }

    void close_pmd()
    {
        pmd_.reset();
    }

    bool pmd_enabled() const
    {
        return pmd_ != nullptr;
    }

    std::size_t
    read_size_hint_pmd(
        std::size_t initial_size,
        bool rd_done,
        std::uint64_t rd_remain,
        detail::frame_header const& rd_fh) const
    {
        using beast::detail::clamp;
        std::size_t result;
        BOOST_ASSERT(initial_size > 0);
        if(! pmd_ || (! rd_done && ! pmd_->rd_set))
        {
            // current message is uncompressed

            if(rd_done)
            {
                // first message frame
                result = initial_size;
                goto done;
            }
            else if(rd_fh.fin)
            {
                // last message frame
                BOOST_ASSERT(rd_remain > 0);
                result = clamp(rd_remain);
                goto done;
            }
        }
        result = (std::max)(
            initial_size, clamp(rd_remain));
    done:
        BOOST_ASSERT(result != 0);
        return result;
    }
};

template<>
struct stream_base<false> : stream_prng
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
        net::mutable_buffer&,
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

    template<class Body, class Allocator>
    void
    build_response_pmd(
        http::response<http::string_body>&,
        http::request<Body,
            http::basic_fields<Allocator>> const&)
    {
    }

    void
    on_response_pmd(
        http::response<http::string_body> const&)
    {
    }

    template<class Allocator>
    void
    do_pmd_config(http::basic_fields<Allocator> const&)
    {
    }

    void
    set_option_pmd(permessage_deflate const& o)
    {
        if(o.client_enable || o.server_enable)
        {
            // Can't enable permessage-deflate
            // when deflateSupported == false.
            //
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "deflateSupported == false"});
        }
    }

    void
    get_option_pmd(permessage_deflate& o)
    {
        o = {};
        o.client_enable = false;
        o.server_enable = false;
    }

    void
    build_request_pmd(
        http::request<http::empty_body>&)
    {
    }

    void open_pmd(role_type)
    {
    }

    void close_pmd()
    {
    }

    bool pmd_enabled() const
    {
        return false;
    }

    std::size_t
    read_size_hint_pmd(
        std::size_t initial_size,
        bool rd_done,
        std::uint64_t rd_remain,
        detail::frame_header const& rd_fh) const
    {
        using beast::detail::clamp;
        std::size_t result;
        BOOST_ASSERT(initial_size > 0);
        // compression is not supported
        if(rd_done)
        {
            // first message frame
            result = initial_size;
        }
        else if(rd_fh.fin)
        {
            // last message frame
            BOOST_ASSERT(rd_remain > 0);
            result = clamp(rd_remain);
        }
        else
        {
            result = (std::max)(
                initial_size, clamp(rd_remain));
        }
        BOOST_ASSERT(result != 0);
        return result;
    }
};

} // detail
} // websocket
} // beast
} // boost

#endif
