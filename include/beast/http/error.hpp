//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_ERROR_HPP
#define BEAST_HTTP_ERROR_HPP

#include <beast/core/error.hpp>

namespace beast {
namespace http {

/// Error codes returned from HTTP parsing
enum class error
{
    /** The end of the stream was reached.

        This error is returned by @ref basic_parser::write_eof
        when the end of stream is reached and there are no
        unparsed bytes in the stream buffer.
    */
    end_of_stream = 1,

    /** The incoming message is incomplete.

        This happens when the end of stream is reached
        and some bytes have been received, but not the
        entire message.
    */
    partial_message,

    /** Buffer maximum exceeded.

        This error is returned when reading HTTP content
        into a dynamic buffer, and the operation would
        exceed the maximum size of the buffer.
    */
    buffer_overflow,

    /// The line ending was malformed
    bad_line_ending,

    /// The method is invalid.
    bad_method,

    /// The request-target is invalid.
    bad_path,

    /// The HTTP-version is invalid.
    bad_version,

    /// The status-code is invalid.
    bad_status,

    /// The reason-phrase is invalid.
    bad_reason,

    /// The field name is invalid.
    bad_field,

    /// The field value is invalid.
    bad_value,

    /// The Content-Length is invalid.
    bad_content_length,

    /// The Transfer-Encoding is invalid.
    bad_transfer_encoding,

    /// The chunk syntax is invalid.
    bad_chunk
};

} // http
} // beast

#include <beast/http/impl/error.ipp>

#endif
