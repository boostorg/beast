//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <example/common/helpers.hpp>
#include <example/common/session_alloc.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/unit_test/dstream.hpp>
#include <boost/asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
using tcp = boost::asio::ip::tcp;
namespace ws = boost::beast::websocket;
namespace ph = std::placeholders;
using error_code = boost::beast::error_code;

class test_buffer : public asio::const_buffers_1
{
    char data_[4096];

public:
    test_buffer()
        : asio::const_buffers_1(data_, sizeof(data_))
    {
        std::mt19937_64 rng;
        std::uniform_int_distribution<unsigned short> dist;
        for(auto& c : data_)
            c = static_cast<unsigned char>(dist(rng));
    }
};

class report
{
    std::mutex m_;
    std::size_t bytes_ = 0;
    std::size_t messages_ = 0;

public:
    void
    insert(std::size_t messages, std::size_t bytes)
    {
        std::lock_guard<std::mutex> lock(m_);
        bytes_ += bytes;
        messages_ += messages;
    }

    std::size_t
    bytes() const
    {
        return bytes_;
    }

    std::size_t
    messages() const
    {
        return messages_;
    }
};

class connection
    : public std::enable_shared_from_this<connection>
{
    std::ostream& log_;
    ws::stream<tcp::socket> ws_;
    tcp::endpoint ep_;
    std::size_t messages_;
    report& rep_;
    test_buffer const& tb_;
    asio::io_service::strand strand_;
    boost::beast::multi_buffer buffer_;
    std::mt19937_64 rng_;
    std::size_t count_ = 0;
    std::size_t bytes_ = 0;
    session_alloc<char> alloc_;

public:
    connection(
        std::ostream& log,
        asio::io_service& ios,
        tcp::endpoint const& ep,
        std::size_t messages,
        bool deflate,
        report& rep,
        test_buffer const& tb)
        : log_(log)
        , ws_(ios)
        , ep_(ep)
        , messages_(messages)
        , rep_(rep)
        , tb_(tb)
        , strand_(ios)
    {
        ws::permessage_deflate pmd;
        pmd.client_enable = deflate;
        ws_.set_option(pmd);
        ws_.binary(true);
        ws_.auto_fragment(false);
        ws_.write_buffer_size(64 * 1024);
    }

    ~connection()
    {
        rep_.insert(count_, bytes_);
    }

    void
    run()
    {
        ws_.next_layer().async_connect(ep_,
            alloc_.wrap(std::bind(
                &connection::on_connect,
                shared_from_this(),
                ph::_1)));
    }

private:
    void
    fail(boost::beast::string_view what, error_code ec)
    {
        if( ec == asio::error::operation_aborted ||
            ec == ws::error::closed)
            return;
        print(log_, "[", ep_, "] ", what, ": ", ec.message());
    }

    void
    on_connect(error_code ec)
    {
        if(ec)
            return fail("on_connect", ec);
        ws_.async_handshake(
            ep_.address().to_string() + ":" + std::to_string(ep_.port()),
            "/",
            alloc_.wrap(std::bind(
                &connection::on_handshake,
                shared_from_this(),
                ph::_1)));
    }

    void
    on_handshake(error_code ec)
    {
        if(ec)
            return fail("on_connect", ec);
        do_write();
    }

    void
    do_write()
    {
        std::geometric_distribution<std::size_t> dist{
            double(4) / boost::asio::buffer_size(tb_)};
        ws_.async_write_some(true,
            boost::beast::buffer_prefix(dist(rng_), tb_),
            alloc_.wrap(std::bind(
                &connection::on_write,
                shared_from_this(),
                ph::_1)));
    }

    void
    on_write(error_code ec)
    {
        if(ec)
            return fail("on_read", ec);
        if(messages_--)
            return do_read();
        ws_.async_close({},
            alloc_.wrap(std::bind(
                &connection::on_close,
                shared_from_this(),
                ph::_1)));
    }

    void
    do_read()
    {
        ws_.async_read(buffer_,
            alloc_.wrap(std::bind(
                &connection::on_read,
                shared_from_this(),
                ph::_1)));
    }

    void
    on_read(error_code ec)
    {
        if(ec)
            return fail("on_read", ec);
        ++count_;
        bytes_ += buffer_.size();
        buffer_.consume(buffer_.size());
        do_write();
    }

    void
    on_close(error_code ec)
    {
        if(ec)
            return fail("on_close", ec);
        do_drain();
    }

    void
    do_drain()
    {
        ws_.async_read(buffer_,
            alloc_.wrap(std::bind(
                &connection::on_drain,
                shared_from_this(),
                ph::_1)));
    }

    void
    on_drain(error_code ec)
    {
        if(ec)
            return fail("on_drain", ec);
        do_drain();
    }

};

class timer
{
    using clock_type =
        std::chrono::system_clock;

    clock_type::time_point when_;

public:
    using duration =
        clock_type::duration;

    timer()
        : when_(clock_type::now())
    {
    }

    duration
    elapsed() const
    {
        return clock_type::now() - when_;
    }
};

inline
std::uint64_t
throughput(
    std::chrono::duration<double> const& elapsed,
    std::uint64_t items)
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        1 / (elapsed/items).count());
}

int
main(int argc, char** argv)
{
    boost::beast::unit_test::dstream dout(std::cerr);

    try
    {
        // Check command line arguments.
        if(argc != 8)
        {
            std::cerr <<
                "Usage: " << argv[0] <<
                " <address> <port> <trials> <messages> <workers> <threads> <compression:0|1>";
            return EXIT_FAILURE;
        }

        auto const address = ip::address::from_string(argv[1]);
        auto const port    = static_cast<unsigned short>(std::atoi(argv[2]));
        auto const trials  = static_cast<std::size_t>(std::atoi(argv[3]));
        auto const messages= static_cast<std::size_t>(std::atoi(argv[4]));
        auto const workers = static_cast<std::size_t>(std::atoi(argv[5]));
        auto const threads = static_cast<std::size_t>(std::atoi(argv[6]));
        auto const deflate = static_cast<bool>(std::atoi(argv[7]));
        auto const work = (messages + workers - 1) / workers;
        test_buffer tb;
        for(auto i = trials; i; --i)
        {
            report rep;
            boost::asio::io_service ios{1};
            for(auto j = workers; j; --j)
            {
                auto sp =
                std::make_shared<connection>(
                    dout,
                    ios,
                    tcp::endpoint{address, port},
                    work,
                    deflate,
                    rep,
                    tb);
                sp->run();
            }
            timer clock;
            std::vector<std::thread> tv;
            if(threads > 1)
            {
                tv.reserve(threads);
                tv.emplace_back([&ios]{ ios.run(); });
            }
            ios.run();
            for(auto& t : tv)
                t.join();
            auto const elapsed = clock.elapsed();
            dout <<
                throughput(elapsed, rep.bytes()) << " bytes/s in " <<
                (std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    elapsed).count() / 1000.) << "ms and " <<
                rep.bytes() << " bytes" << std::endl;
        }
    }
    catch(std::exception const& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
