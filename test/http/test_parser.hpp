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
    : public basic_parser<
        isRequest, test_parser<isRequest>>
{
    test::fail_counter* fc_ = nullptr;

public:
    int status = 0;
    int version = 0;
    std::string method;
    std::string path;
    std::string reason;
    std::string body;
    bool got_on_begin_message   = false;
    bool got_on_field           = false;
    bool got_on_end_header      = false;
    bool got_on_begin_body      = false;
    bool got_on_chunk           = false;
    bool got_on_body            = false;
    bool got_on_end_body        = false;
    bool got_on_end_message     = false;

    test_parser() = default;

    explicit
    test_parser(test::fail_counter& fc)
        : fc_(&fc)
    {
    }

    void
    split(bool option)
    {
        basic_parser<isRequest,
            test_parser<isRequest>>::split(option);
    }

    void
    on_begin_request(
        boost::string_ref const& method_,
            boost::string_ref const& path_,
                int version_, error_code& ec)
    {
        method = std::string(
            method_.data(), method_.size());
        path = std::string(
            path_.data(), path_.size());
        version = version_;
        got_on_begin_message = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_begin_response(int status_,
        boost::string_ref const& reason_,
            int version_, error_code& ec)
    {
        status = status_;
        reason = std::string(
            reason_.data(), reason_.size());
        version = version_;
        got_on_begin_message = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_field(boost::string_ref const&,
        boost::string_ref const&,
            error_code& ec)
    {
        got_on_field = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_end_header(error_code& ec)
    {
        got_on_end_header = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_begin_body(error_code& ec)
    {
        got_on_begin_body = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_chunk(std::uint64_t,
        boost::string_ref const&,
            error_code& ec)
    {
        got_on_chunk = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_body(boost::string_ref const& s,
        error_code& ec)
    {
        got_on_body = true;
        if(fc_ && fc_->fail(ec))
            return;
        body.append(s.data(), s.size());
    }

    void
    on_end_body(error_code& ec)
    {
        got_on_end_body = true;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_end_message(error_code& ec)
    {
        got_on_end_message = true;
        if(fc_)
            fc_->fail(ec);
    }
};

} // http
} // beast

#endif
