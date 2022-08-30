//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: JSON body
//
//------------------------------------------------------------------------------

#ifndef BOOST_BEAST_EXAMPLE_JSON_BODY
#define BOOST_BEAST_EXAMPLE_JSON_BODY

#include <boost/json.hpp>
#include <boost/json/stream_parser.hpp>
#include <boost/json/monotonic_resource.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/buffer.hpp>

namespace json = boost::json;

struct json_body
{
    using value_type = json::value;

    struct writer
    {
        using const_buffers_type = boost::asio::const_buffer;
        template<bool isRequest, class Fields>
        writer(boost::beast::http::header<isRequest, Fields> const& h, 
               value_type const& body)
        {
            serializer.reset(&body);
        }

        void
        init(boost::system::error_code& ec)
        {
            // The specification requires this to indicate "no error"
            ec = {};
        }
    
        boost::optional<std::pair<const_buffers_type, bool>>
        get(boost::system::error_code& ec)
        {
            ec = {};
            const auto len = serializer.read(buffer, sizeof(buffer));
            return std::make_pair(
                boost::asio::const_buffer(len.data(), len.size()), 
                    !serializer.done()); 
        }
      private:
        json::serializer serializer;
        // half of the probably networking buffer, let's leave some space for headers
        char buffer[32768]; 
    };

    struct reader
    {
        template<bool isRequest, class Fields>
        reader(boost::beast::http::header<isRequest, Fields>& h, value_type& body)
            : body(body)
        {
        }
        void
        init(
            boost::optional<std::uint64_t> const& content_length,
            boost::system::error_code& ec)
        {
            
            // The specification requires this to indicate "no error"
            if (content_length)
                parser.reset(json::make_shared_resource<json::monotonic_resource>(*content_length));
            ec = {};
        }

        template<class ConstBufferSequence>
        std::size_t
        put(ConstBufferSequence const& buffers, boost::system::error_code& ec)
        {
            // The specification requires this to indicate "no error"
            ec = {};

            return parser.write_some(
                static_cast<const char*>(buffers.data()), buffers.size(), ec);
        }

        void
        finish(boost::system::error_code& ec)
        {
            // The specification requires this to indicate "no error"
            ec = {};
            if (parser.done())
                body = parser.release();
            else
                ec = boost::json::error::incomplete;
        }

      private:
        json::stream_parser parser;
        value_type& body;
    };
};


#endif