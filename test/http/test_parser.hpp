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
    : public basic_parser<isRequest, false,
        test_parser<isRequest>>
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
    bool got_on_prepare     = false;
    bool got_on_commit      = false;
    bool got_on_chunk       = false;
    bool got_on_complete    = false;

    test_parser() = default;

    explicit
    test_parser(test::fail_counter& fc)
        : fc_(&fc)
    {
    }

    void
    on_request(
        string_view const& method_,
            string_view const& path_,
                int version_, error_code& ec)
    {
        method = std::string(
            method_.data(), method_.size());
        path = std::string(
            path_.data(), path_.size());
        version = version_;
        got_on_begin = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_response(int status_,
        string_view const& reason_,
            int version_, error_code& ec)
    {
        status = status_;
        reason = std::string(
            reason_.data(), reason_.size());
        version = version_;
        got_on_begin = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_field(string_view const&,
        string_view const&,
            error_code& ec)
    {
        got_on_field = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_header(error_code& ec)
    {
        got_on_header = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_body(error_code& ec)
    {
        got_on_body = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_body(std::uint64_t content_length,
        error_code& ec)
    {
        got_on_body = true;
        got_content_length = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_data(string_view const& s,
        error_code& ec)
    {
        body.append(s.data(), s.size());
    }

    void
    on_chunk(std::uint64_t,
        string_view const&,
            error_code& ec)
    {
        got_on_chunk = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_complete(error_code& ec)
    {
        got_on_complete = true;
        if(fc_)
            fc_->fail(ec);
    }
};

} // http
} // beast

#endif
