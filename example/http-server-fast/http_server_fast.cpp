//
// Copyright (c) 2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "fields_alloc.hpp"

#include "../common/mime_types.hpp"

#include <beast/core.hpp>
#include <beast/http.hpp>
#include <beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <string>

namespace ip = boost::asio::ip; // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio.hpp>
namespace http = beast::http; // from <beast/http.hpp>

class http_worker
{
public:
    http_worker(http_worker const&) = delete;
    http_worker& operator=(http_worker const&) = delete;

    http_worker(tcp::acceptor& acceptor, const std::string& doc_root) :
        acceptor_(acceptor),
        doc_root_(doc_root),
        socket_(acceptor.get_io_service()),
        alloc_(8192),
        request_deadline_(acceptor.get_io_service(),
            std::chrono::steady_clock::time_point::max())
    {
    }

    void start()
    {
        accept();
        check_deadline();
    }

private:
    using request_body_t = http::basic_dynamic_body<beast::static_buffer_n<1024 * 1024>>;

    // The acceptor used to listen for incoming connections.
    tcp::acceptor& acceptor_;

    // The path to the root of the document directory.
    std::string doc_root_;

    // The socket for the currently connected client.
    tcp::socket socket_;

    // The buffer for performing reads
    beast::static_buffer_n<8192> buffer_;

    // The parser for reading the requests
    using alloc_type = fields_alloc<char>;
    alloc_type alloc_;
    boost::optional<http::request_parser<request_body_t, alloc_type>> parser_;

    // The timer putting a time limit on requests.
    boost::asio::basic_waitable_timer<std::chrono::steady_clock> request_deadline_;

    // The response message.
    http::response<http::string_body> response_;

    // The response serializer.
    boost::optional<http::response_serializer<http::string_body>> serializer_;

    void accept()
    {
        // Clean up any previous connection.
        beast::error_code ec;
        socket_.close(ec);
        buffer_.consume(buffer_.size());

        acceptor_.async_accept(
            socket_,
            [this](beast::error_code ec)
            {
                if (ec)
                {
                    accept();
                }
                else
                {
                    // Request must be fully processed within 60 seconds.
                    request_deadline_.expires_from_now(
                        std::chrono::seconds(60));

                    read_header();
                }
            });
    }

    void read_header()
    {
        // On each read the parser needs to be destroyed and
        // recreated. We store it in a boost::optional to
        // achieve that.
        //
        // Arguments passed to the parser constructor are
        // forwarded to the message object. A single argument
        // is forwarded to the body constructor.
        //
        // We construct the dynamic body with a 1MB limit
        // to prevent vulnerability to buffer attacks.
        //
        parser_.emplace(
            std::piecewise_construct,
            std::make_tuple(),
            std::make_tuple(alloc_));

        http::async_read_header(
            socket_,
            buffer_,
            *parser_,
            [this](beast::error_code ec)
            {
                if (ec)
                    accept();
                else
                    read_body();
            });
    }

    void read_body()
    {
        http::async_read(
            socket_,
            buffer_,
            *parser_,
            [this](beast::error_code ec)
            {
                if (ec)
                    accept();
                else
                    process_request(parser_->get());
            });
    }

    void process_request(http::request<request_body_t, http::basic_fields<alloc_type>> const& req)
    {
        response_.version = 11;
        response_.set(http::field::connection, "close");

        switch (req.method())
        {
        case http::verb::get:
            response_.result(http::status::ok);
            response_.set(http::field::server, "Beast");
            load_file(req.target());
            break;

        default:
            // We return responses indicating an error if
            // we do not recognize the request method.
            response_.result(http::status::bad_request);
            response_.set(http::field::content_type, "text/plain");
            response_.body = "Invalid request-method '" + req.method_string().to_string() + "'";
            response_.prepare_payload();
            break;
        }

        write_response();
    }

    void load_file(beast::string_view target)
    {
        // Request path must be absolute and not contain "..".
        if (target.empty() || target[0] != '/' || target.find("..") != std::string::npos)
        {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            response_.body = "File not found\r\n";
            return;
        }

        std::string full_path = doc_root_;
        full_path.append(target.data(), target.size());

        // Open the file to send back.
        std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
        if (!is)
        {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            response_.body = "File not found\r\n";
            return;
        }

        // Fill out the reply to be sent to the client.
        response_.set(http::field::content_type, mime_type(target.to_string()));
        response_.body.clear();
        for (char buf[2048]; is.read(buf, sizeof(buf)).gcount() > 0;)
            response_.body.append(buf, static_cast<std::size_t>(is.gcount()));
        response_.prepare_payload();
    }

    void write_response()
    {
        response_.set(http::field::content_length, response_.body.size());

        serializer_.emplace(response_);

        http::async_write(
            socket_,
            *serializer_,
            [this](beast::error_code ec)
            {
                socket_.shutdown(tcp::socket::shutdown_send, ec);
                accept();
            });
    }

    void check_deadline()
    {
        // The deadline may have moved, so check it has really passed.
        if (request_deadline_.expires_at() <= std::chrono::steady_clock::now())
        {
            // Close socket to cancel any outstanding operation.
            beast::error_code ec;
            socket_.close();

            // Sleep indefinitely until we're given a new deadline.
            request_deadline_.expires_at(
                std::chrono::steady_clock::time_point::max());
        }

        request_deadline_.async_wait(
            [this](beast::error_code)
            {
                check_deadline();
            });
    }
};

int main(int argc, char* argv[])
{
    try
    {
        // Check command line arguments.
        if (argc != 5)
        {
            std::cerr << "Usage: http_server <address> <port> <doc_root> <num_workers>\n";
            std::cerr << "  For IPv4, try:\n";
            std::cerr << "    receiver 0.0.0.0 80 . 100\n";
            std::cerr << "  For IPv6, try:\n";
            std::cerr << "    receiver 0::0 80 . 100\n";
            return EXIT_FAILURE;
        }

        auto address = ip::address::from_string(argv[1]);
        unsigned short port = static_cast<unsigned short>(std::atoi(argv[2]));
        std::string doc_root = argv[3];
        int num_workers = std::atoi(argv[4]);

        boost::asio::io_service ios{1};
        tcp::acceptor acceptor{ios, {address, port}};

        std::list<http_worker> workers;
        for (int i = 0; i < num_workers; ++i)
        {
            workers.emplace_back(acceptor, doc_root);
            workers.back().start();
        }

        ios.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
