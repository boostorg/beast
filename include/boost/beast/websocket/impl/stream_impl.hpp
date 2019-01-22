//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_STREAM_IMPL_HPP
#define BOOST_BEAST_WEBSOCKET_IMPL_STREAM_IMPL_HPP

#include <boost/beast/core/static_buffer.hpp>
#include <boost/beast/core/saved_handler.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/beast/websocket/detail/pmd_extension.hpp>
#include <boost/beast/websocket/detail/soft_mutex.hpp>
#include <boost/beast/websocket/detail/utf8_checker.hpp>
#include <boost/optional.hpp>

namespace boost {
namespace beast {
namespace websocket {

template<
    class NextLayer, bool deflateSupported>
struct stream<NextLayer, deflateSupported>::impl_type
    : std::enable_shared_from_this<impl_type>
    , detail::impl_base<deflateSupported>
{
    using time_point = typename
        std::chrono::steady_clock::time_point;

    static constexpr time_point never()
    {
        return (time_point::max)();
    }

    NextLayer               stream;         // The underlying stream
    close_reason            cr;             // set from received close frame
    control_cb_type         ctrl_cb;        // control callback

    std::size_t             rd_msg_max      /* max message size */ = 16 * 1024 * 1024;
    std::uint64_t           rd_size         /* total size of current message so far */ = 0;
    std::uint64_t           rd_remain       /* message frame bytes left in current frame */ = 0;
    detail::frame_header    rd_fh;          // current frame header
    detail::prepared_key    rd_key;         // current stateful mask key
    detail::frame_buffer    rd_fb;          // to write control frames (during reads)
    detail::utf8_checker    rd_utf8;        // to validate utf8
    static_buffer<
        +tcp_frame_size>    rd_buf;         // buffer for reads
    detail::opcode          rd_op           /* current message binary or text */ = detail::opcode::text;
    bool                    rd_cont         /* `true` if the next frame is a continuation */ = false;
    bool                    rd_done         /* set when a message is done */ = true;
    bool                    rd_close        /* did we read a close frame? */ = false;
    detail::soft_mutex      rd_block;       // op currently reading

    role_type               role            /* server or client */ = role_type::client;
    status                  status_         /* state of the object */ = status::closed;

    detail::soft_mutex      wr_block;       // op currently writing
    bool                    wr_close        /* did we write a close frame? */ = false;
    bool                    wr_cont         /* next write is a continuation */ = false;
    bool                    wr_frag         /* autofrag the current message */ = false;
    bool                    wr_frag_opt     /* autofrag option setting */ = true;
    bool                    wr_compress     /* compress current message */ = false;
    detail::opcode          wr_opcode       /* message type */ = detail::opcode::text;
    std::unique_ptr<
        std::uint8_t[]>     wr_buf;         // write buffer
    std::size_t             wr_buf_size     /* write buffer size (current message) */ = 0;
    std::size_t             wr_buf_opt      /* write buffer size option setting */ = 4096;
    detail::fh_buffer       wr_fb;          // header buffer used for writes

    saved_handler           paused_rd;      // paused read op
    saved_handler           paused_wr;      // paused write op
    saved_handler           paused_ping;    // paused ping op
    saved_handler           paused_close;   // paused close op
    saved_handler           paused_r_rd;    // paused read op (async read)
    saved_handler           paused_r_close; // paused close op (async read)

    template<class... Args>
    impl_type(Args&&... args)
        : stream(std::forward<Args>(args)...)
    {
    }

    void
    open(role_type role_)
    {
        // VFALCO TODO analyze and remove dupe code in reset()
        role = role_;
        status_ = status::open;
        rd_remain = 0;
        rd_cont = false;
        rd_done = true;
        // Can't clear this because accept uses it
        //rd_buf.reset();
        rd_fh.fin = false;
        rd_close = false;
        wr_close = false;
        // These should not be necessary, because all completion
        // handlers must be allowed to execute otherwise the
        // stream exhibits undefined behavior.
        wr_block.reset();
        rd_block.reset();
        cr.code = close_code::none;

        wr_cont = false;
        wr_buf_size = 0;

        this->open_pmd(role);
    }

    void
    close()
    {
        wr_buf.reset();
        this->close_pmd();
    }

    void
    reset()
    {
        BOOST_ASSERT(status_ != status::open);
        rd_remain = 0;
        rd_cont = false;
        rd_done = true;
        rd_buf.consume(rd_buf.size());
        rd_fh.fin = false;
        rd_close = false;
        wr_close = false;
        wr_cont = false;
        // These should not be necessary, because all completion
        // handlers must be allowed to execute otherwise the
        // stream exhibits undefined behavior.
        wr_block.reset();
        rd_block.reset();
        cr.code = close_code::none;
    }

    // Called before each write frame
    void
    begin_msg()
    {
        wr_frag = wr_frag_opt;

        // Maintain the write buffer
        if( this->pmd_enabled() ||
            role == role_type::client)
        {
            if(! wr_buf ||
                wr_buf_size != wr_buf_opt)
            {
                wr_buf_size = wr_buf_opt;
                wr_buf = boost::make_unique_noinit<
                    std::uint8_t[]>(wr_buf_size);
            }
        }
        else
        {
            wr_buf_size = wr_buf_opt;
            wr_buf.reset();
        }
    }

    bool
    check_open(error_code& ec)
    {
        if(status_ != status::open)
        {
            ec = net::error::operation_aborted;
            return false;
        }
        ec = {};
        return true;
    }

    bool
    check_ok(error_code& ec)
    {
        if(ec)
        {
            if(status_ != status::closed)
                status_ = status::failed;
            return false;
        }
        return true;
    }
};

} // websocket
} // beast
} // boost

#endif
