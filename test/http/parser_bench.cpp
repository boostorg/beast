//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "nodejs_parser.hpp"
#include "message_fuzz.hpp"

#include <beast/http.hpp>
#include <beast/core/consuming_buffers.hpp>
#include <beast/core/streambuf.hpp>
#include <beast/core/to_string.hpp>
#include <beast/unit_test/suite.hpp>
#include <chrono>
#include <iostream>
#include <vector>

namespace beast {
namespace http {

class parser_bench_test : public beast::unit_test::suite
{
public:
    static std::size_t constexpr N = 2000;

    using corpus = std::vector<streambuf>;

    corpus creq_;
    corpus cres_;
    std::size_t size_ = 0;

    parser_bench_test()
    {
        creq_ = build_corpus(N/2, std::true_type{});
        cres_ = build_corpus(N/2, std::false_type{});
    }

    corpus
    build_corpus(std::size_t n, std::true_type)
    {
        corpus v;
        v.resize(N);
        message_fuzz mg;
        for(std::size_t i = 0; i < n; ++i)
        {
            mg.request(v[i]);
            size_ += v[i].size();
        }
        return v;
    }

    corpus
    build_corpus(std::size_t n, std::false_type)
    {
        corpus v;
        v.resize(N);
        message_fuzz mg;
        for(std::size_t i = 0; i < n; ++i)
        {
            mg.response(v[i]);
            size_ += v[i].size();
        }
        return v;
    }

    template<class ConstBufferSequence,
        bool isRequest, bool isDirect, class Derived>
    static
    std::size_t
    feed(ConstBufferSequence const& buffers,
        basic_parser<isRequest, isDirect, Derived>& parser,
            error_code& ec)
    {
        using boost::asio::buffer_size;
        beast::consuming_buffers<
            ConstBufferSequence> cb{buffers};
        std::size_t used = 0;
        for(;;)
        {
            auto const n =
                parser.write(cb, ec);
            if(ec)
                return 0;
            if(n == 0)
                break;
            cb.consume(n);
            used += n;
            if(parser.is_complete())
                break;
            if(buffer_size(cb) == 0)
                break;
        }
        return used;
    }

    template<class Parser>
    void
    testParser1(std::size_t repeat, corpus const& v)
    {
        while(repeat--)
            for(auto const& sb : v)
            {
                Parser p;
                error_code ec;
                p.write(sb.data(), ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    log << to_string(sb.data()) << std::endl;
            }
    }

    template<class Parser>
    void
    testParser2(std::size_t repeat, corpus const& v)
    {
        while(repeat--)
            for(auto const& sb : v)
            {
                Parser p;
                error_code ec;
                feed(sb.data(), p, ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    log << to_string(sb.data()) << std::endl;
            }
    }

    template<class Function>
    void
    timedTest(std::size_t repeat, std::string const& name, Function&& f)
    {
        using namespace std::chrono;
        using clock_type = std::chrono::high_resolution_clock;
        log << name << std::endl;
        for(std::size_t trial = 1; trial <= repeat; ++trial)
        {
            auto const t0 = clock_type::now();
            f();
            auto const elapsed = clock_type::now() - t0;
            log <<
                "Trial " << trial << ": " <<
                duration_cast<milliseconds>(elapsed).count() << " ms" << std::endl;
        }
    }

    template<bool isRequest>
    struct null_parser :
        basic_parser<isRequest, true,
            null_parser<isRequest>>
    {
    };

    template<bool isRequest, class Body, class Fields>
    struct bench_parser : basic_parser<
        isRequest, false, bench_parser<isRequest, Body, Fields>>
    {
        using mutable_buffers_type =
            boost::asio::mutable_buffers_1;

        void
        on_request(boost::string_ref const&,
            boost::string_ref const&,
                int, error_code&)
        {
        }

        void
        on_response(int,
            boost::string_ref const&,
                int, error_code&)
        {
        }

        void
        on_field(boost::string_ref const&,
            boost::string_ref const&,
                error_code&)
        {
        }

        void
        on_header(error_code& ec)
        {
        }

        void
        on_body(error_code& ec)
        {
        }

        void
        on_body(std::uint64_t content_length,
            error_code& ec)
        {
        }

        void
        on_data(boost::string_ref const&,
            error_code& ec)
        {
        }

        void
        on_chunk(std::uint64_t,
            boost::string_ref const&,
                error_code&)
        {
        }

        void
        on_body(boost::string_ref const&,
            error_code&)
        {
        }

        void
        on_complete(error_code&)
        {
        }
    };

    void
    testSpeed()
    {
        static std::size_t constexpr Trials = 3;
        static std::size_t constexpr Repeat = 500;

        log << "sizeof(request parser)  == " <<
            sizeof(null_parser<true>) << '\n';

        log << "sizeof(response parser) == " <<
            sizeof(null_parser<false>)<< '\n';

        testcase << "Parser speed test, " <<
            ((Repeat * size_ + 512) / 1024) << "KB in " <<
                (Repeat * (creq_.size() + cres_.size())) << " messages";

        timedTest(Trials, "nodejs_parser",
            [&]
            {
                testParser1<nodejs_parser<
                    true, dynamic_body, fields>>(
                        Repeat, creq_);
                testParser1<nodejs_parser<
                    false, dynamic_body, fields>>(
                        Repeat, cres_);
            });
        timedTest(Trials, "http::basic_parser",
            [&]
            {
                testParser2<bench_parser<
                    true, dynamic_body, fields> >(
                        Repeat, creq_);
                testParser2<bench_parser<
                    false, dynamic_body, fields>>(
                        Repeat, cres_);
            });
        pass();
    }

    void run() override
    {
        pass();
        testSpeed();
    }
};

BEAST_DEFINE_TESTSUITE(parser_bench,http,beast);

} // http
} // beast

