//
// Copyright (c) 2016-2026 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

module;

#include <boost/beast.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/_experimental/http/icy_stream.hpp>
#include <boost/beast/_experimental/test/error.hpp>
#include <boost/beast/_experimental/test/fail_count.hpp>
#include <boost/beast/_experimental/test/handler.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

export module boost.beast;

export namespace boost::beast {
    using boost::beast::async_base;
    using boost::beast::basic_stream;
    using boost::beast::file;
    using boost::beast::file_mode;
    #if BOOST_BEAST_USE_POSIX_FILE
    using boost::beast::file_posix;
    #endif
    using boost::beast::file_stdio;
    #if BOOST_BEAST_USE_WIN32_FILE
    using boost::beast::file_win32;
    #endif
    using boost::beast::flat_stream;
    using boost::beast::iequal;
    using boost::beast::iless;
    using boost::beast::rate_policy_access;
    using boost::beast::saved_handler;
    using boost::beast::simple_rate_policy;
    using boost::beast::static_string;
    using boost::beast::stable_async_base;
    using boost::beast::string_view;
    using boost::beast::tcp_stream;
    using boost::beast::unlimited_rate_policy;

    using boost::beast::condition;
    using boost::beast::error;
    using boost::beast::file_mode;
    using boost::beast::role_type;

    using boost::beast::allocate_stable;
    using boost::beast::async_detect_ssl;
    using boost::beast::beast_close_socket;
    using boost::beast::bind_front_handler;
    using boost::beast::bind_handler;

    template <class Socket>
    inline void close_socket(Socket& sock) {
        boost::beast::close_socket(sock);
    }

    using boost::beast::detect_ssl;
    using boost::beast::generic_category;
    using boost::beast::get_lowest_layer;
    using boost::beast::iequals;
    using boost::beast::to_static_string;

    using boost::beast::executor_type;
    using boost::beast::has_get_executor;
    using boost::beast::is_async_read_stream;
    template <class T>
    using is_async_stream = std::integral_constant<bool,
        is_async_read_stream<T>::value && is_async_write_stream<T>::value>;
    using boost::beast::is_async_write_stream;
    
    using boost::beast::is_file;
    using boost::beast::is_sync_read_stream;
    template <class T>
    using is_sync_stream = std::integral_constant<bool,
        is_sync_read_stream<T>::value && is_sync_write_stream<T>::value>;
    using boost::beast::is_sync_write_stream;
    // using boost::beast::lowest_layer_type;

    // using boost::beast::ssl_stream;

    namespace errc = boost::beast::errc;
    using boost::beast::error_category;
    using boost::beast::error_code;
    using boost::beast::error_condition;
    using boost::beast::system_category;
    using boost::beast::system_error;

    using boost::beast::basic_flat_buffer;
    using boost::beast::basic_multi_buffer;
    // using boost::beast::buffer_ref;
    using boost::beast::buffered_read_stream;
    using boost::beast::buffers_adaptor;
    using boost::beast::buffers_cat_view;
    using boost::beast::buffers_prefix_view;
    using boost::beast::buffers_prefix_view;
    using boost::beast::buffers_suffix;
    using boost::beast::flat_buffer;
    using boost::beast::flat_static_buffer;
    using boost::beast::flat_static_buffer_base;
    using boost::beast::multi_buffer;
    using boost::beast::static_buffer;
    using boost::beast::static_buffer_base;

    using boost::beast::async_write;

    template <class BufferSequence>
    [[nodiscard]]
    inline std::size_t buffer_bytes(BufferSequence const& buffers) {
        return boost::beast::buffer_bytes(buffers);
    }

    using boost::beast::buffers_cat;
    using boost::beast::buffers_front;
    using boost::beast::buffers_prefix;
    using boost::beast::buffers_range;
    using boost::beast::buffers_range_ref;
    using boost::beast::buffers_to_string;
    using boost::beast::make_printable;
    using boost::beast::ostream;
    using boost::beast::read_size;
    using boost::beast::read_size_or_throw;
    // using boost::beast::ref;
    using boost::beast::write;

    using boost::beast::buffers_iterator_type;
    // using boost::beast::buffers_type;
    using boost::beast::is_buffers_generator;
    // using boost::beast::is_const_buffer_sequence;
    // using boost::beast::is_mutable_buffer_sequence;

    namespace http {
        using boost::beast::http::basic_chunk_extensions;
        using boost::beast::http::basic_dynamic_body;
        using boost::beast::http::basic_fields;
        using boost::beast::http::basic_file_body;
        using boost::beast::http::basic_parser;
        using boost::beast::http::basic_string_body;
        using boost::beast::http::buffer_body;
        using boost::beast::http::chunk_body;
        using boost::beast::http::chunk_crlf;
        using boost::beast::http::chunk_extensions;
        using boost::beast::http::chunk_header;
        using boost::beast::http::chunk_last;
        using boost::beast::http::dynamic_body;
        using boost::beast::http::empty_body;
        using boost::beast::http::fields;
        using boost::beast::http::file_body;
        using boost::beast::http::header;
        using boost::beast::http::message;
        using boost::beast::http::message_generator;
        using boost::beast::http::parser;
        using boost::beast::http::request;
        using boost::beast::http::request_header;
        using boost::beast::http::request_serializer;
        using boost::beast::http::response;
        using boost::beast::http::response_header;
        using boost::beast::http::response_parser;
        using boost::beast::http::response_serializer;
        using boost::beast::http::serializer;
        using boost::beast::http::span_body;
        using string_body = boost::beast::http::basic_string_body<char>;
        using boost::beast::http::vector_body;

        using boost::beast::http::async_read;
        using boost::beast::http::async_read_header;
        using boost::beast::http::async_read_some;
        using boost::beast::http::async_write;
        using boost::beast::http::async_write_header;
        using boost::beast::http::async_write_some;
        using boost::beast::http::int_to_status;
        using boost::beast::http::make_chunk;
        using boost::beast::http::make_chunk_last;
        using boost::beast::http::obsolete_reason;
        using boost::beast::http::operator<<;
        using boost::beast::http::read;
        using boost::beast::http::read_header;
        using boost::beast::http::read_some;
        using boost::beast::http::string_to_field;
        using boost::beast::http::string_to_verb;
        using boost::beast::http::swap;
        using boost::beast::http::to_string;
        using boost::beast::http::to_status_class;
        using boost::beast::http::write;
        using boost::beast::http::write_header;
        using boost::beast::http::write_some;

        using boost::beast::http::error;
        using boost::beast::http::field;
        using boost::beast::http::status;
        using boost::beast::http::status_class;
        using boost::beast::http::verb;
        
        // using boost::beast::http::is_body;
        using boost::beast::http::is_body_reader;
        using boost::beast::http::is_body_writer;
        using boost::beast::http::is_fields;
        using boost::beast::http::is_mutable_body_writer;
        
        using boost::beast::http::ext_list;
        using boost::beast::http::opt_token_list;
        using boost::beast::http::param_list;
        using boost::beast::http::token_list;
    }

    namespace websocket {
        using boost::beast::websocket::close_reason;
        using ping_data = boost::beast::static_string<125, char>;
        using boost::beast::websocket::stream;
        using boost::beast::websocket::stream_base;
        using reason_string = boost::beast::static_string<123, char>;
        
        using boost::beast::websocket::async_teardown;
        using boost::beast::websocket::is_upgrade;
        using boost::beast::websocket::seed_prng;
        using boost::beast::websocket::teardown;

        using boost::beast::websocket::permessage_deflate;
        
        using boost::beast::websocket::close_code;
        using boost::beast::websocket::condition;
        using boost::beast::websocket::error;
        using boost::beast::websocket::frame_type;
    }

    namespace zlib {
        using boost::beast::zlib::deflate_stream;
        using boost::beast::zlib::inflate_stream;
        using boost::beast::zlib::z_params;
        using boost::beast::zlib::deflate_upper_bound;
        using boost::beast::zlib::error;
        using boost::beast::zlib::Flush;
        using boost::beast::zlib::Strategy;
    }

    namespace http {
        using boost::beast::http::icy_stream;
    }
    
    namespace test {
        using boost::beast::test::fail_count;
        using boost::beast::test::handler;
        // using boost::beast::test::stream;
        using boost::beast::test::connect;
        using boost::beast::test::any_handler;
        using boost::beast::test::fail_handler;
        using boost::beast::test::success_handler;
        using boost::beast::test::error;
    }
}
