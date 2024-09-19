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
        writer(
            boost::beast::http::header<isRequest, Fields> const&,
            value_type const& body)
        {
            // The serializer holds a pointer to the value, so all we need to do is to reset it.
            serializer.reset(&body);
        }

        void
        init(boost::system::error_code& ec)
        {
            // The serializer always works, so no error can occur here.
            ec = {};
        }
    
        boost::optional<std::pair<const_buffers_type, bool>>
        get(boost::system::error_code& ec)
        {
            ec = {};
            // We serialize as much as we can with the buffer. Often that'll suffice
            const auto len = serializer.read(buffer, sizeof(buffer));
            return std::make_pair(
                boost::asio::const_buffer(len.data(), len.size()), 
                    !serializer.done()); 
        }
      private:
        json::serializer serializer;
        // half of the probable networking buffer, let's leave some space for headers
        char buffer[32768]; 
    };

    struct reader
    {
        template<bool isRequest, class Fields>
        reader(
            boost::beast::http::header<isRequest, Fields>&,
            value_type& body)
            : body(body)
        {
        }
        void
        init(
            boost::optional<std::uint64_t> const& content_length,
            boost::system::error_code& ec)
        {
            
            // If we know the content-length, we can allocate a monotonic resource to increase the parsing speed.
            // We're using it rather then a static_resource, so a consumer can modify the resulting value.
            // It is also only assumption that the parsed json will be smaller than the serialize one,
            // it might not always be the case.
            if (content_length)
                parser.reset(json::make_shared_resource<json::monotonic_resource>(*content_length));
            ec = {};
        }

        template<class ConstBufferSequence>
        std::size_t
        put(ConstBufferSequence const& buffers, boost::system::error_code& ec)
        {
            ec = {};
            // The parser just uses the `ec` to indicate errors, so we don't need to do anything.
            return parser.write_some(
                static_cast<const char*>(buffers.data()), buffers.size(), ec);
        }

        void
        finish(boost::system::error_code& ec)
        {
            ec = {};
            // We check manually if the json is complete.
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