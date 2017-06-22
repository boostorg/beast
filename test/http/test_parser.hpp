//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_TEST_PARSER_HPP
#define BEAST_HTTP_TEST_PARSER_HPP

#include <beast/http/basic_parser.hpp>
#include <beast/test/fail_counter.hpp>

namespace beast {
namespace http {

template<bool isRequest>
class test_parser
    : public basic_parser<isRequest, test_parser<isRequest>>
{
    test::fail_counter* fc_ = nullptr;

public:
    using mutable_buffers_type =
        boost::asio::mutable_buffers_1;

    int status = 0;
    int version = 0;
    std::string method;
    std::string path;
    std::string reason;
    std::string body;
    bool got_on_begin       = false;
    bool got_on_field       = false;
    bool got_on_header      = false;
    bool got_on_body        = false;
    bool got_content_length = false;
    bool got_on_chunk       = false;
    bool got_on_complete    = false;

    test_parser() = default;

    explicit
    test_parser(test::fail_counter& fc)
        : fc_(&fc)
    {
    }

    void
    on_request(verb, string_view method_str_,
        string_view path_, int version_, error_code& ec)
    {
        method = std::string(
            method_str_.data(), method_str_.size());
        path = std::string(
            path_.data(), path_.size());
        version = version_;
        got_on_begin = true;
        if(fc_)
            fc_->fail(ec);
        else
            ec.assign(0, ec.category());
    }

    void
    on_response(int code,
        string_view reason_,
            int version_, error_code& ec)
    {
        status = code;
        reason = std::string(
            reason_.data(), reason_.size());
        version = version_;
        got_on_begin = true;
        if(fc_)
            fc_->fail(ec);
        else
            ec.assign(0, ec.category());
    }

    void
    on_field(field, string_view,
        string_view, error_code& ec)
    {
        got_on_field = true;
        if(fc_)
            fc_->fail(ec);
        else
            ec.assign(0, ec.category());
    }

    void
    on_header(error_code& ec)
    {
        got_on_header = true;
        if(fc_)
            fc_->fail(ec);
        else
            ec.assign(0, ec.category());
    }

    void
    on_body(boost::optional<
        std::uint64_t> const& content_length_,
            error_code& ec)
    {
        got_on_body = true;
        got_content_length =
            static_cast<bool>(content_length_);
        if(fc_)
            fc_->fail(ec);
        else
            ec.assign(0, ec.category());
    }

    void
    on_data(string_view s,
        error_code& ec)
    {
        body.append(s.data(), s.size());
        if(fc_)
            fc_->fail(ec);
        else
            ec.assign(0, ec.category());
    }

    void
    on_chunk(std::uint64_t,
        string_view, error_code& ec)
    {
        got_on_chunk = true;
        if(fc_)
            fc_->fail(ec);
        else
            ec.assign(0, ec.category());
    }

    void
    on_complete(error_code& ec)
    {
        got_on_complete = true;
        if(fc_)
            fc_->fail(ec);
        else
            ec.assign(0, ec.category());
    }
};

} // http
} // beast

#endif
