//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core/error.hpp>
#include <beast/http/message.hpp>
#include <beast/http/type_traits.hpp>
#include <boost/optional.hpp>
#include <cstdint>
#include <utility>

namespace beast {
namespace http {

class BodyReader;
class BodyWriter;

//[concept_Body

struct Body
{
    // The type of message::body when used
    struct value_type;

    /// The algorithm used for extracting buffers
    using reader = BodyReader;

    /// The algorithm used for inserting buffers
    using writer = BodyWriter;

    /// Returns the body's payload size
    static
    std::uint64_t
    size(value_type const& v);
};

static_assert(is_body<Body>::value, "");

//]

struct Body_BodyReader {
    struct value_type{};
//[concept_BodyReader

struct BodyReader
{
public:
    /// The type of buffer returned by `get`.
    using const_buffers_type = boost::asio::const_buffers_1;

    /** Construct the reader.

        @param msg The message whose body is to be retrieved.

        @param ec Set to the error, if any occurred.
    */
    template<bool isRequest, class Body, class Headers>
    explicit
    BodyReader(message<isRequest, Body, Headers> const& msg,
        error_code &ec);

    /** Returns the next buffer in the body.

        @li If the return value is `boost::none` (unseated optional) and
            `ec` does not contain an error, this indicates the end of the
            body, no more buffers are present.

        @li If the optional contains a value, the first element of the
            pair represents a @b ConstBufferSequence containing one or
            more octets of the body data. The second element indicates
            whether or not there are additional octets of body data.
            A value of `true` means there is more data, and that the
            implementation will perform a subsequent call to `get`.
            A value of `false` means there is no more body data.

        @li If `ec` contains an error code, the return value is ignored.

        @param ec Set to the error, if any occurred.
    */
    boost::optional<std::pair<const_buffers_type, bool>>
    get(error_code& ec)
    {
        // The specification requires this to indicate "no error"
        ec = {};

        return boost::none; // for exposition only
    }
};

//]
    using reader = BodyReader;
};

static_assert(is_body_reader<Body_BodyReader>::value, "");

struct Body_BodyWriter {
    struct value_type{};
//[concept_BodyWriter

struct BodyWriter
{
    /** Construct the writer.

        @param msg The message whose body is to be stored.
    */
    template<bool isRequest, class Body, class Fields>
    explicit
    BodyWriter(message<isRequest, Body, Fields>& msg,
        boost::optional<std::uint64_t> content_length,
            error_code& ec)
    {
        boost::ignore_unused(msg, content_length);

        // The specification requires this to indicate "no error"
        ec = {};
    }

    /** Store buffers.

        This is called zero or more times with parsed body octets.

        @param buffers The constant buffer sequence to store.

        @param ec Set to the error, if any occurred.
    */
    template<class ConstBufferSequence>
    void
    put(ConstBufferSequence const& buffers, error_code& ec)
    {
        // The specification requires this to indicate "no error"
        ec = {};
    }

    /** Called when the body is complete.

        @param ec Set to the error, if any occurred.
    */
    void
    finish(error_code& ec)
    {
        // The specification requires this to indicate "no error"
        ec = {};
    }
};

//]
    using writer = BodyWriter;
};

static_assert(is_body_writer<Body_BodyWriter>::value, "");

//[concept_Fields

class Fields
{
public:
    struct reader;

protected:
    /** Set or clear the method string.

        @note Only called for requests.
    */
    void set_method_impl(string_view s);

    /** Set or clear the target string.

        @note Only called for requests.
    */
    void set_target_impl(string_view s);

    /** Set or clear the reason string.

        @note Only called for responses.
    */
    void set_reason_impl(string_view s);

    /** Returns the request-method string.

        @note Only called for requests.
    */
    string_view get_method_impl() const;

    /** Returns the request-target string.

        @note Only called for requests.
    */
    string_view get_target_impl() const;

    /** Returns the response reason-phrase string.

        @note Only called for responses.
    */
    string_view get_reason_impl() const;

    /** Updates the payload metadata.

        @param b `true` if chunked

        @param n The content length if known, otherwise `boost::none`
    */
    void prepare_payload_impl(bool b, boost::optional<std::uint64_t> n);
};

static_assert(is_fields<Fields>::value, "");

//]

struct Fields_FieldsReader {
    using F = Fields_FieldsReader;
//[concept_FieldsReader

struct FieldsReader
{
    // The type of buffers returned by `get`
    struct const_buffers_type;

    // Constructor for requests
    FieldsReader(F const& f, unsigned version, verb method);

    // Constructor for responses
    FieldsReader(F const& f, unsigned version, unsigned status);

    // Returns `true` if the payload uses the chunked Transfer-Encoding
    bool
    chunked();

    // Returns `true` if keep-alive is indicated
    bool
    keep_alive();

    // Returns the serialized header buffers
    const_buffers_type
    get();
};

//]
};

} // http
} // beast
