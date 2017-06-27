//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_SERIALIZER_HPP
#define BEAST_HTTP_SERIALIZER_HPP

#include <beast/config.hpp>
#include <beast/core/buffer_cat.hpp>
#include <beast/core/consuming_buffers.hpp>
#include <beast/core/string.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/http/message.hpp>
#include <beast/http/detail/chunk_encode.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#ifndef BEAST_NO_BIG_VARIANTS
# if defined(BOOST_GCC) && BOOST_GCC < 50000 && BOOST_VERSION < 106400
#  define BEAST_NO_BIG_VARIANTS 1
# else
#  define BEAST_NO_BIG_VARIANTS 0
# endif
#endif

namespace beast {
namespace http {

/** A chunk decorator which does nothing.

    When selected as a chunk decorator, objects of this type
    affect the output of messages specifying chunked
    transfer encodings as follows:

    @li chunk headers will have empty chunk extensions, and

    @li final chunks will have an empty set of trailers.

    @see @ref serializer
*/
struct no_chunk_decorator
{
    template<class ConstBufferSequence>
    string_view
    operator()(ConstBufferSequence const&) const
    {
        return {};
    }

    string_view
    operator()(boost::asio::null_buffers) const
    {
        return {};
    }
};

/** Provides buffer oriented HTTP message serialization functionality.

    An object of this type is used to serialize a complete
    HTTP message into a sequence of octets. To use this class,
    construct an instance with the message to be serialized.

    The implementation will automatically perform chunk encoding
    if the contents of the message indicate that chunk encoding
    is required. If the semantics of the message indicate that
    the connection should be closed after the message is sent, the
    function @ref need_close will return `true`.

    Upon construction, an optional chunk decorator may be
    specified. This decorator is a function object called with
    each buffer sequence of the body when the chunked transfer
    encoding is indicate in the message header. The decorator
    will be called with an empty buffer sequence (actually
    the type `boost::asio::null_buffers`) to indicate the
    final chunk. The decorator may return a string which forms
    the chunk extension for chunks, and the field trailers
    for the final chunk.

    In C++11 the decorator must be declared as a class or
    struct with a templated operator() thusly:

    @code
    // The implementation guarantees that operator()
    // will be called only after the view returned by
    // any previous calls to operator() are no longer
    // needed. The decorator instance is intended to
    // manage the lifetime of the storage for all returned
    // views.
    //
    struct decorator
    {
        // Returns the chunk-extension for each chunk,
        // or an empty string for no chunk extension. The
        // buffer must include the leading semicolon (";")
        // and follow the format for chunk extensions defined
        // in rfc7230.
        //
        template<class ConstBufferSequence>
        string_view
        operator()(ConstBufferSequence const&) const;

        // Returns a set of field trailers for the final chunk.
        // Each field should be formatted according to rfc7230
        // including the trailing "\r\n" for each field. If
        // no trailers are indicated, an empty string is returned.
        //
        string_view
        operator()(boost::asio::null_buffers) const;
    };
    @endcode

    @tparam isRequest `true` if the message is a request.

    @tparam Body The body type of the message.

    @tparam Fields The type of fields in the message.

    @tparam ChunkDecorator The type of chunk decorator to use.
*/
template<
    bool isRequest,
    class Body,
    class Fields = fields,
    class ChunkDecorator = no_chunk_decorator>
class serializer
{
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");

    enum
    {
        do_construct        =   0,

        do_init             =  10,
        do_header_only      =  20,
        do_header           =  30,
        do_body             =  40,
        
        do_init_c           =  50,
        do_header_only_c    =  60,
        do_header_c         =  70,
        do_body_c           =  80,
        do_final_c          =  90,
    #if ! BEAST_NO_BIG_VARIANTS
        do_body_final_c     = 100,
        do_all_c            = 110,
    #endif

        do_complete         = 120
    };

    void frdinit(std::true_type);
    void frdinit(std::false_type);

    using reader = typename Body::reader;

    using ch_t = consuming_buffers<typename
        Fields::reader::const_buffers_type>;        // header

    using cb0_t = consuming_buffers<buffer_cat_view<
        typename Fields::reader::const_buffers_type,// header
        typename reader::const_buffers_type>>;      // body

    using cb1_t = consuming_buffers<
        typename reader::const_buffers_type>;       // body

    using ch0_t = consuming_buffers<buffer_cat_view<
        typename Fields::reader::const_buffers_type,// header
        detail::chunk_header,                       // chunk-header
        boost::asio::const_buffers_1,               // chunk-ext
        boost::asio::const_buffers_1,               // crlf
        typename reader::const_buffers_type,        // body
        boost::asio::const_buffers_1>>;             // crlf

    using ch1_t = consuming_buffers<buffer_cat_view<
        detail::chunk_header,                       // chunk-header
        boost::asio::const_buffers_1,               // chunk-ext
        boost::asio::const_buffers_1,               // crlf
        typename reader::const_buffers_type,        // body
        boost::asio::const_buffers_1>>;             // crlf

#if ! BEAST_NO_BIG_VARIANTS
    using ch2_t = consuming_buffers<buffer_cat_view<
        detail::chunk_header,                       // chunk-header
        boost::asio::const_buffers_1,               // chunk-ext
        boost::asio::const_buffers_1,               // crlf
        typename reader::const_buffers_type,        // body
        boost::asio::const_buffers_1,               // crlf
        boost::asio::const_buffers_1,               // chunk-final
        boost::asio::const_buffers_1,               // trailers 
        boost::asio::const_buffers_1>>;             // crlf

    using ch3_t = consuming_buffers<buffer_cat_view<
        typename Fields::reader::const_buffers_type,// header
        detail::chunk_header,                       // chunk-header
        boost::asio::const_buffers_1,               // chunk-ext
        boost::asio::const_buffers_1,               // crlf
        typename reader::const_buffers_type,        // body
        boost::asio::const_buffers_1,               // crlf
        boost::asio::const_buffers_1,               // chunk-final
        boost::asio::const_buffers_1,               // trailers 
        boost::asio::const_buffers_1>>;             // crlf
#endif

    using ch4_t = consuming_buffers<buffer_cat_view<
        boost::asio::const_buffers_1,               // chunk-final
        boost::asio::const_buffers_1,               // trailers 
        boost::asio::const_buffers_1>>;             // crlf

    message<isRequest, Body, Fields> const& m_;
    boost::optional<typename Fields::reader> frd_;
    boost::optional<reader> rd_;
    boost::variant<boost::blank,
        ch_t, cb0_t, cb1_t, ch0_t, ch1_t
    #if ! BEAST_NO_BIG_VARIANTS
        ,ch2_t, ch3_t
    #endif
        , ch4_t> v_;
    int s_ = do_construct;
    bool split_ = false;
    bool header_done_ = false;
    bool chunked_;
    bool close_;
    bool more_;
    ChunkDecorator d_;

public:
    /** Constructor

        The implementation guarantees that the message passed on
        construction will not be accessed until the first call to
        @ref get. This allows the message to be lazily created.
        For example, if the header is filled in before serialization.

        @param msg The message to serialize, which must remain valid
        for the lifetime of the serializer.

        @param decorator An optional decorator to use.
    */
    explicit
    serializer(message<isRequest, Body, Fields> const& msg,
        ChunkDecorator const& decorator = ChunkDecorator{});

    /** Returns `true` if we will pause after writing the complete header.
    */
    bool
    split() const
    {
        return split_;
    }

    /** Set whether the header and body are written separately.

        When the split feature is enabled, the implementation will
        write only the octets corresponding to the serialized header
        first. If the header has already been written, this function
        will have no effect on output.
    */
    void
    split(bool v)
    {
        split_ = v;
    }

    /** Return `true` if serialization of the header is complete.

        This function indicates whether or not all buffers containing
        serialized header octets have been retrieved.
    */
    bool
    is_header_done() const
    {
        return header_done_;
    }

    /** Return `true` if serialization is complete.

        The operation is complete when all octets corresponding
        to the serialized representation of the message have been
        successfully retrieved.
    */
    bool
    is_done() const
    {
        return s_ == do_complete;
    }

    /** Return `true` if Connection: close semantic is indicated.

        Depending on the contents of the message, the end of
        the body may be indicated by the end of file. In order
        for the recipient (if any) to receive a complete message,
        the underlying network connection must be closed when this
        function returns `true`.
    */
    bool
    need_close() const
    {
        return close_;
    }

    /** Returns the next set of buffers in the serialization.

        This function will attempt to call the `visit` function
        object with a @b ConstBufferSequence of unspecified type
        representing the next set of buffers in the serialization
        of the message represented by this object. 

        If there are no more buffers in the serialization, the
        visit function will not be called. In this case, no error
        will be indicated, and the function @ref is_done will
        return `true`.

        @param ec Set to the error, if any occurred.

        @param visit The function to call. The equivalent function
        signature of this object must be:
        @code
            template<class ConstBufferSequence>
            void visit(error_code&, ConstBufferSequence const&);
        @endcode
        The function is not copied, if no error occurs it will be
        invoked before the call to @ref get returns.

    */
    template<class Visit>
    void
    get(error_code& ec, Visit&& visit);

    /** Consume buffer octets in the serialization.

        This function should be called after one or more octets
        contained in the buffers provided in the prior call
        to @ref get have been used.

        After a call to @ref consume, callers should check the
        return value of @ref is_done to determine if the entire
        message has been serialized.

        @param n The number of octets to consume. This number must
        be greater than zero and no greater than the number of
        octets in the buffers provided in the prior call to @ref get.
    */
    void
    consume(std::size_t n);
};

/// A serializer for HTTP/1 requests
template<
    class Body,
    class Fields = fields,
    class ChunkDecorator = no_chunk_decorator>
using request_serializer = serializer<true, Body, Fields, ChunkDecorator>;

/// A serializer for HTTP/1 responses
template<
    class Body,
    class Fields = fields,
    class ChunkDecorator = no_chunk_decorator>
using response_serializer = serializer<false, Body, Fields, ChunkDecorator>;

} // http
} // beast

#include <beast/http/impl/serializer.ipp>

#endif
