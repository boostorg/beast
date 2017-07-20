//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_SERIALIZER_HPP
#define BOOST_BEAST_HTTP_SERIALIZER_HPP

#include <boost/beast/config.hpp>
#include <boost/beast/core/buffer_cat.hpp>
#include <boost/beast/core/buffer_prefix.hpp>
#include <boost/beast/core/consuming_buffers.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/chunk_encode.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#ifndef BOOST_BEAST_NO_BIG_VARIANTS
# if defined(BOOST_GCC) && BOOST_GCC < 50000 && BOOST_VERSION < 106600
#  define BOOST_BEAST_NO_BIG_VARIANTS
# endif
#endif

namespace boost {
namespace beast {
namespace http {

/** Provides buffer oriented HTTP message serialization functionality.

    An object of this type is used to serialize a complete
    HTTP message into a sequence of octets. To use this class,
    construct an instance with the message to be serialized.

    The implementation will automatically perform chunk encoding
    if the contents of the message indicate that chunk encoding
    is required. If the semantics of the message indicate that
    the connection should be closed after the message is sent, the
    function @ref keep_alive will return `false`.

    Chunked output produced by the serializer never contains chunk
    extensions or trailers, and the location of chunk boundaries
    is not specified. If callers require chunk extensions, trailers,
    or control over the exact contents of each chunk they should
    use the serializer to write just the message header, and then
    assume control over serializing the chunked payload by using
    the chunk buffer sequence types @ref chunk_body, @ref chunk_crlf,
    @ref chunk_header, and @ref chunk_last.

    @tparam isRequest `true` if the message is a request.

    @tparam Body The body type of the message.

    @tparam Fields The type of fields in the message.
*/
template<
    bool isRequest,
    class Body,
    class Fields = fields>
class serializer
{
public:
    static_assert(is_body<Body>::value,
        "Body requirements not met");

    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");

    /** The type of message this serializer uses

        This may be const or non-const depending on the
        implementation of the corresponding @b BodyReader.
    */
#if BOOST_BEAST_DOXYGEN
    using value_type = implementation_defined;
#else
    using value_type =
        typename std::conditional<
            std::is_constructible<typename Body::reader,
                message<isRequest, Body, Fields>&>::value &&
            ! std::is_constructible<typename Body::reader,
                message<isRequest, Body, Fields> const&>::value,
            message<isRequest, Body, Fields>,
            message<isRequest, Body, Fields> const>::type;
#endif

private:
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
    #ifndef BOOST_BEAST_NO_BIG_VARIANTS
        do_body_final_c     = 100,
        do_all_c            = 110,
    #endif

        do_complete         = 120
    };

    void frdinit(std::true_type);
    void frdinit(std::false_type);

    template<class T1, class T2, class Visit>
    void
    do_visit(error_code& ec, Visit& visit);

    using reader = typename Body::reader;

    using cb1_t = consuming_buffers<typename
        Fields::reader::const_buffers_type>;        // header
    using pcb1_t  = buffer_prefix_view<cb1_t const&>;

    using cb2_t = consuming_buffers<buffer_cat_view<
        typename Fields::reader::const_buffers_type,// header
        typename reader::const_buffers_type>>;      // body
    using pcb2_t = buffer_prefix_view<cb2_t const&>;

    using cb3_t = consuming_buffers<
        typename reader::const_buffers_type>;       // body
    using pcb3_t = buffer_prefix_view<cb3_t const&>;

    using cb4_t = consuming_buffers<buffer_cat_view<
        typename Fields::reader::const_buffers_type,// header
        detail::chunk_size,                         // chunk-size
        boost::asio::const_buffers_1,               // chunk-ext
        chunk_crlf,                                 // crlf
        typename reader::const_buffers_type,        // body
        chunk_crlf>>;                               // crlf
    using pcb4_t = buffer_prefix_view<cb4_t const&>;

    using cb5_t = consuming_buffers<buffer_cat_view<
        detail::chunk_size,                         // chunk-header
        boost::asio::const_buffers_1,               // chunk-ext
        chunk_crlf,                                 // crlf
        typename reader::const_buffers_type,        // body
        chunk_crlf>>;                               // crlf
    using pcb5_t = buffer_prefix_view<cb5_t const&>;

#ifndef BOOST_BEAST_NO_BIG_VARIANTS
    using cb6_t = consuming_buffers<buffer_cat_view<
        detail::chunk_size,                         // chunk-header
        boost::asio::const_buffers_1,               // chunk-size
        chunk_crlf,                                 // crlf
        typename reader::const_buffers_type,        // body
        chunk_crlf,                                 // crlf
        boost::asio::const_buffers_1,               // chunk-final
        boost::asio::const_buffers_1,               // trailers 
        chunk_crlf>>;                               // crlf
    using pcb6_t = buffer_prefix_view<cb6_t const&>;

    using cb7_t = consuming_buffers<buffer_cat_view<
        typename Fields::reader::const_buffers_type,// header
        detail::chunk_size,                         // chunk-size
        boost::asio::const_buffers_1,               // chunk-ext
        chunk_crlf,                                 // crlf
        typename reader::const_buffers_type,        // body
        chunk_crlf,                                 // crlf
        boost::asio::const_buffers_1,               // chunk-final
        boost::asio::const_buffers_1,               // trailers 
        chunk_crlf>>;                               // crlf
    using pcb7_t = buffer_prefix_view<cb7_t const&>;
#endif

    using cb8_t = consuming_buffers<buffer_cat_view<
        boost::asio::const_buffers_1,               // chunk-final
        boost::asio::const_buffers_1,               // trailers 
        chunk_crlf>>;                               // crlf
    using pcb8_t = buffer_prefix_view<cb8_t const&>;

    value_type& m_;
    reader rd_;
    boost::optional<typename Fields::reader> frd_;
    boost::variant<boost::blank,
        cb1_t, cb2_t, cb3_t, cb4_t, cb5_t
    #ifndef BOOST_BEAST_NO_BIG_VARIANTS
        ,cb6_t, cb7_t
    #endif
        , cb8_t> v_;
    boost::variant<boost::blank,
        pcb1_t, pcb2_t, pcb3_t, pcb4_t, pcb5_t
    #ifndef BOOST_BEAST_NO_BIG_VARIANTS
        ,pcb6_t, pcb7_t
    #endif
        , pcb8_t> pv_;
    std::size_t limit_ =
        (std::numeric_limits<std::size_t>::max)();
    int s_ = do_construct;
    bool split_ = false;
    bool header_done_ = false;
    bool chunked_;
    bool keep_alive_;
    bool more_;

public:
    /** Constructor

        The implementation guarantees that the message passed on
        construction will not be accessed until the first call to
        @ref next. This allows the message to be lazily created.
        For example, if the header is filled in before serialization.

        @param msg A reference to the message to serialize, which must
        remain valid for the lifetime of the serializer. Depending on
        the type of Body used, this may or may not be a `const` reference.

        @note This function participates in overload resolution only if
        Body::reader is constructible from a `const` message reference.
    */
    explicit
    serializer(value_type& msg);

    /// Returns the message being serialized
    value_type&
    get()
    {
        return m_;
    }

    /// Returns the serialized buffer size limit
    std::size_t
    limit()
    {
        return limit_;
    }

    /** Set the serialized buffer size limit

        This function adjusts the limit on the maximum size of the
        buffers passed to the visitor. The new size limit takes effect
        in the following call to @ref next.

        The default is no buffer size limit.

        @param limit The new buffer size limit. If this number
        is zero, the size limit is removed.
    */
    void
    limit(std::size_t limit)
    {
        limit_ = limit > 0 ? limit :
            (std::numeric_limits<std::size_t>::max)();
    }

    /** Returns `true` if we will pause after writing the complete header.
    */
    bool
    split()
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
    is_header_done()
    {
        return header_done_;
    }

    /** Return `true` if serialization is complete.

        The operation is complete when all octets corresponding
        to the serialized representation of the message have been
        successfully retrieved.
    */
    bool
    is_done()
    {
        return s_ == do_complete;
    }

    /** Return `true` if the serializer will apply chunk-encoding.

        This function may only be called if @ref is_header_done
        would return `true`.
    */
    bool
    chunked()
    {
        return chunked_;
    }

    /** Return `true` if Connection: keep-alive semantic is indicated.

        This function returns `true` if the semantics of the
        message indicate that the connection should be kept open
        after the serialized message has been transmitted. The
        value depends on the HTTP version of the message,
        the tokens in the Connection header, and the metadata
        describing the payload body.

        Depending on the payload body, the end of the message may
        be indicated by connection closuire. In order for the
        recipient (if any) to receive a complete message, the
        underlying stream or network connection must be closed
        when this function returns `false`.

        This function may only be called if @ref is_header_done
        would return `true`.
    */
    bool
    keep_alive()
    {
        return keep_alive_;
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
        invoked before the call to @ref next returns.

    */
    template<class Visit>
    void
    next(error_code& ec, Visit&& visit);

    /** Consume buffer octets in the serialization.

        This function should be called after one or more octets
        contained in the buffers provided in the prior call
        to @ref next have been used.

        After a call to @ref consume, callers should check the
        return value of @ref is_done to determine if the entire
        message has been serialized.

        @param n The number of octets to consume. This number must
        be greater than zero and no greater than the number of
        octets in the buffers provided in the prior call to @ref next.
    */
    void
    consume(std::size_t n);

    /** Provides low-level access to the associated @b BodyReader

        This function provides access to the instance of the reader
        associated with the body and created by the serializer
        upon construction. The behavior of accessing this object
        is defined by the specification of the particular reader
        and its associated body.

        @return A reference to the reader.
    */
    reader&
    reader_impl()
    {
        return rd_;
    }
};

/// A serializer for HTTP/1 requests
template<class Body, class Fields = fields>
using request_serializer = serializer<true, Body, Fields>;

/// A serializer for HTTP/1 responses
template<class Body, class Fields = fields>
using response_serializer = serializer<false, Body, Fields>;

} // http
} // beast
} // boost

#include <boost/beast/http/impl/serializer.ipp>

#endif
