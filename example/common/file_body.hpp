//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_COMMON_FILE_BODY_HPP
#define BEAST_EXAMPLE_COMMON_FILE_BODY_HPP

#include <beast/core/error.hpp>
#include <beast/http/message.hpp>
#include <boost/filesystem.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <utility>

//[example_http_file_body_1

/** A message body represented by a file on the filesystem.

    Messages with this type have bodies represented by a
    file on the file system. When parsing a message using
    this body type, the data is stored in the file pointed
    to by the path, which must be writable. When serializing,
    the implementation will read the file and present those
    octets as the body content. This may be used to serve
    content from a directory as part of a web service.
*/
struct file_body
{
    /** The type of the @ref message::body member.

        Messages declared using `file_body` will have this
        type for the body member. We use a path indicating
        the location on the file system for which the data
        will be read or written.  
    */
    using value_type = boost::filesystem::path;

    /** Returns the content length of the body in a message.

        This optional static function returns the size of the
        body in bytes. It is called from @ref message::size to
        return the payload size, and from @ref message::prepare
        to automatically set the Content-Length field. If this
        function is omitted from a body type, calls to
        @ref message::prepare will set the chunked transfer
        encoding.

        @param m The message containing a file body to check.

        @return The size of the file in bytes.
    */
    static
    std::uint64_t
    size(value_type const& v);

    /** Algorithm for retrieving buffers when serializing.

        Objects of this type are created during serialization
        to extract the buffers representing the body.
    */
    class reader;

    /** Algorithm for storing buffers when parsing.

        Objects of this type are created during parsing
        to store incoming buffers representing the body.
    */
    class writer;
};

//]

//[example_http_file_body_2

inline
std::uint64_t
file_body::
size(value_type const& v)
{
    return boost::filesystem::file_size(v);
}

//]

//[example_http_file_body_3

class file_body::reader
{
    value_type const& path_;    // Path of the file
    FILE* file_ = nullptr;      // File handle
    std::uint64_t remain_ = 0;  // The number of unread bytes
    char buf_[4096];            // Small buffer for reading

public:
    // The type of buffer sequence returned by `get`.
    //
    using const_buffers_type =
        boost::asio::const_buffers_1;

    // Constructor.
    //
    // `m` holds the message we are sending, which will
    // always have the `file_body` as the body type.
    //
    template<bool isRequest, class Fields>
    reader(beast::http::message<isRequest, file_body, Fields> const& m,
        beast::error_code& ec);

    // Destructor
    ~reader();

    // This function is called zero or more times to
    // retrieve buffers. A return value of `boost::none`
    // means there are no more buffers. Otherwise,
    // the contained pair will have the next buffer
    // to serialize, and a `bool` indicating whether
    // or not there may be additional buffers.
    boost::optional<std::pair<const_buffers_type, bool>>
    get(beast::error_code& ec);
};

//]

//[example_http_file_body_4

// Here we just stash a reference to the path for later.
// Rather than dealing with messy constructor exceptions,
// we save the things that might fail for the call to `init`.
//
template<bool isRequest, class Fields>
file_body::reader::
reader(beast::http::message<isRequest, file_body, Fields> const& m,
        beast::error_code& ec)
    : path_(m.body)
{
    // Attempt to open the file for reading
    file_ = fopen(path_.string().c_str(), "rb");

    if(! file_)
    {
        // Convert the old-school `errno` into
        // an error code using the generic category.
        ec = beast::error_code{errno, beast::generic_category()};
        return;
    }

    // This is required by the error_code specification
    ec = {};

    // The file was opened successfully, get the size
    // of the file to know how much we need to read.
    remain_ = boost::filesystem::file_size(path_);
}

// This function is called repeatedly by the serializer to
// retrieve the buffers representing the body. Our strategy
// is to read into our buffer and return it until we have
// read through the whole file.
//
inline
auto
file_body::reader::
get(beast::error_code& ec) ->
    boost::optional<std::pair<const_buffers_type, bool>>
{
    // Calculate the smaller of our buffer size,
    // or the amount of unread data in the file.
    auto const amount =  remain_ > sizeof(buf_) ?
        sizeof(buf_) : static_cast<std::size_t>(remain_);

    // Check for an empty file
    if(amount == 0)
    {
        ec = {};
        return boost::none;
    }

    // Now read the next buffer
    auto const nread = fread(buf_, 1, amount, file_);

    // Handle any errors
    if(ferror(file_))
    {
        // Convert the old-school `errno` into
        // an error code using the generic category.
        ec = beast::error_code{errno, beast::generic_category()};
        return boost::none;
    }

    // Make sure there is forward progress
    BOOST_ASSERT(nread != 0);
    BOOST_ASSERT(nread <= remain_);

    // Update the amount remaining based on what we got
    remain_ -= nread;

    // Return the buffer to the caller.
    //
    // The second element of the pair indicates whether or
    // not there is more data. As long as there is some
    // unread bytes, there will be more data. Otherwise,
    // we set this bool to `false` so we will not be called
    // again.
    //
    ec = {};
    return {{
        const_buffers_type{buf_, nread},    // buffer to return.
        remain_ > 0                         // `true` if there are more buffers.
        }};
}

// The destructor is always invoked if construction succeeds.
//
inline
file_body::reader::
~reader()
{
    // Just close the file if its open
    if(file_)
        fclose(file_);

    // In theory fclose() can fail but how would we handle it?
}

//]

//[example_http_file_body_5

class file_body::writer
{
    value_type const& path_;    // A path to the file
    FILE* file_ = nullptr;      // The file handle

public:
    // Constructor.
    //
    // This is called after the header is parsed and
    // indicates that a non-zero sized body may be present.
    // `m` holds the message we are receiving, which will
    // always have the `file_body` as the body type.
    //
    template<bool isRequest, class Fields>
    explicit
    writer(beast::http::message<isRequest, file_body, Fields>& m,
        boost::optional<std::uint64_t> const& content_length,
            beast::error_code& ec);

    // This function is called one or more times to store
    // buffer sequences corresponding to the incoming body.
    //
    template<class ConstBufferSequence>
    void
    put(ConstBufferSequence const& buffers, beast::error_code& ec);

    // This function is called when writing is complete.
    // It is an opportunity to perform any final actions
    // which might fail, in order to return an error code.
    // Operations that might fail should not be attemped in
    // destructors, since an exception thrown from there
    // would terminate the program.
    //
    void
    finish(beast::error_code& ec);

    // Destructor.
    //
    // Avoid calling anything that might fail here.
    //
    ~writer();
};

//]

//[example_http_file_body_6

// Just stash a reference to the path so we can open the file later.
template<bool isRequest, class Fields>
file_body::writer::
writer(beast::http::message<isRequest, file_body, Fields>& m,
    boost::optional<std::uint64_t> const& content_length,
        beast::error_code& ec)
    : path_(m.body)
{
    boost::ignore_unused(content_length);

    // Attempt to open the file for writing
    file_ = fopen(path_.string().c_str(), "wb");

    if(! file_)
    {
        // Convert the old-school `errno` into
        // an error code using the generic category.
        ec = beast::error_code{errno, beast::generic_category()};
        return;
    }

    // This is required by the error_code specification
    ec = {};
}

// This will get called one or more times with body buffers
//
template<class ConstBufferSequence>
void
file_body::writer::
put(ConstBufferSequence const& buffers, beast::error_code& ec)
{
    // Loop over all the buffers in the sequence,
    // and write each one to the file.
    for(boost::asio::const_buffer buffer : buffers)
    {
        // Write this buffer to the file
        fwrite(
            boost::asio::buffer_cast<void const*>(buffer), 1,
            boost::asio::buffer_size(buffer),
            file_);

        // Handle any errors
        if(ferror(file_))
        {
            // Convert the old-school `errno` into
            // an error code using the generic category.
            ec = beast::error_code{errno, beast::generic_category()};
            return;
        }
    }

    // Indicate success
    ec = {};
}

// Called after writing is done when there's no error.
inline
void
file_body::writer::
finish(beast::error_code& ec)
{
    // This has to be cleared before returning, to
    // indicate no error. The specification requires it.
    ec = {};
}

// The destructor is always invoked if construction succeeds
//
inline
file_body::writer::
~writer()
{
    // Just close the file if its open
    if(file_)
        fclose(file_);

    // In theory fclose() can fail but how would we handle it?
}

//]

#endif
