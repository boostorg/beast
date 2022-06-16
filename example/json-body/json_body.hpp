//
// Copyright (c) 2022 Seth Heeren (sgheeren at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_JSON_BODY_JSON_BODY_HPP
#define BOOST_BEAST_EXAMPLE_JSON_BODY_JSON_BODY_HPP

#include <boost/asio/buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/json.hpp>
#include <boost/optional.hpp>

namespace boost {
namespace beast {
namespace contrib {

/** A <em>Body</em> using a @ref boost::json::value

    This body uses a @ref boost::json::value as a memory-based container for
    holding message payloads. Messages using this body type may be serialized
    and parsed.

    The writer implicitly requires chunked encoding to avoid counting
    content-length. If you need non-chunked encoding, use @ref
    http::string_body after @ref json::serialize
*/
template<
    unsigned max_depth = 32,
    bool allow_comments = true,
    bool allow_trailing_commas = true,
    bool allow_invalid_utf8 = false>
struct basic_json_body
{
    /** The type of container used for the body

        This determines the type of @ref message::body
        when this body type is used with a message container.
    */
    using value_type = boost::json::value;

    /** The algorithm for parsing the body

        Meets the requirements of <em>BodyReader</em>.
    */
    class reader
    {
        value_type& body_;

        //unsigned char scratch_[128];
        boost::optional<boost::json::stream_parser> parser_;

    public:
        template<bool isRequest, class Fields>
        explicit
        reader(http::header<isRequest, Fields>&, value_type& b)
            : body_(b)
        {
        }

        void
        init(boost::optional<std::uint64_t> const&, error_code& ec)
        {
            boost::json::parse_options opts;
            opts.max_depth = max_depth;
            opts.allow_comments = allow_comments;
            opts.allow_trailing_commas = allow_trailing_commas;
            opts.allow_invalid_utf8 = allow_invalid_utf8;

            parser_.emplace(json::storage_ptr{}, opts);
            ec = {};
        }

        template<class ConstBufferSequence>
        std::size_t
        put(ConstBufferSequence const& buffers, error_code& ec)
        {
            size_t n = 0;
            for(auto bb = net::buffer_sequence_begin(buffers),
                     be = net::buffer_sequence_end(buffers);
                bb != be;
                ++bb)
            {
                n += parser_->write(
                    static_cast<char const*>(bb->data()), bb->size(), ec);
                if(ec)
                    break;
            }
            return ec.failed() ? 0 : n;
        }

        void
        finish(error_code& ec)
        {
            parser_->finish(ec);
            if(! ec)
            {
                body_ = parser_->release();
            }
        }
    };

    /** The algorithm for serializing the body

        Meets the requirements of <em>BodyWriter</em>.
    */
    class writer
    {
        value_type const& body_;

        boost::json::serializer ser_;
        char buf_[128];

    public:
        using const_buffers_type = std::array<net::const_buffer, 1>;

        template<bool isRequest, class Fields>
        explicit
        writer(http::header<isRequest, Fields> const&, value_type const& b)
            : body_(b)
        {
        }

        void
        init(error_code& ec)
        {
            ser_.reset(&body_);
            ec = {};
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            auto sv = ser_.read(buf_);
            ec = {};
            return boost::make_optional(std::make_pair(
                const_buffers_type{net::buffer(sv.data(), sv.size())},
                ! ser_.done()));
        }
    };
};

using json_body = basic_json_body<>;

} // contrib
} // beast
} // boost

#endif
