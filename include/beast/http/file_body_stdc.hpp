//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_FILE_BODY_STDC_HPP
#define BEAST_HTTP_FILE_BODY_STDC_HPP

#include <beast/core/error.hpp>
#include <beast/core/file_base.hpp>
#include <beast/http/message.hpp>
#include <boost/filesystem.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <utility>

namespace beast {
namespace http {

//[example_http_file_body_stdc_1

/** A message body represented by a file on the filesystem.

    Messages with this type have bodies represented by a
    file on the file system. When parsing a message using
    this body type, the data is stored in the file pointed
    to by the path, which must be writable. When serializing,
    the implementation will read the file and present those
    octets as the body content. This may be used to serve
    content from a directory as part of a web service.
*/
struct file_body_stdc
{
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

    /** The type of the @ref message::body member.

        Messages declared using `file_body_stdc` will have this
        type for the body member. This rich class interface
        allow the file to be opened with the file handle
        maintained directly in the object, which is attached
        to the message.
    */
    class value_type;
};

//]

//[example_http_file_body_stdc_2

// The body container holds a handle to the file if
// it is open, and we also cache the size upon opening.
//
class file_body_stdc::value_type
{
    friend class reader;
    friend class writer;

    FILE* file_ = nullptr;
    std::uint64_t size_{0};

public:
    /** Destructor.

        If the file handle is open, it is closed first.
    */
    ~value_type()
    {
        if(file_)
            fclose(file_);
    }

    /// Default constructor
    value_type() = default;

    /// Move constructor
    value_type(value_type&& other)
        : file_(other.file_)
        , size_(other.size_)
    {
        other.file_ = nullptr;
    }

    /// Move assignment
    value_type& operator=(value_type&& other)
    {
        file_ = other.file_;
        size_ = other.size_;
        other.file_ = nullptr;
        return *this;
    }

    /// Returns `true` if the file is open
    bool
    is_open() const
    {
        return file_ != nullptr;
    }

    /** Open a file for reading or writing.

        @param path The path to the file

        @param mode The open mode used with fopen()

        @param ec Set to the error, if any occurred
    */
    void
    open(boost::filesystem::path const& path,
        file_mode mode, beast::error_code& ec)
    {
        // Attempt to open the file for reading
        char const* s;
        switch(mode)
        {
        default:
        case file_mode::read:   s = "rb"; break;
        case file_mode::scan:   s = "rb"; break;
        case file_mode::write:  s = "wb"; break;
        case file_mode::append: s = "wb+"; break;
        }
        file_ = fopen(path.string().c_str(), s);

        if(! file_)
        {
            // Convert the old-school `errno` into
            // an error code using the generic category.
            ec = beast::error_code{errno, beast::generic_category()};
            return;
        }

        // The file was opened successfully.
        size_ = boost::filesystem::file_size(path, ec);
    }

    /** Returns the size of the file.

        Preconditions:

        * The file must be open for binary reading (mode=="rb")

        @return The size of the file if a file is open, else undefined.
    */
    std::uint64_t
    size() const
    {
        return size_;
    }
};

//]

//[example_http_file_body_stdc_3

class file_body_stdc::reader
{
    FILE* file_;                // The file handle
    std::uint64_t remain_;      // The number of unread bytes
    char buf_[4096];            // Small buffer for reading

public:
    // The type of buffer sequence returned by `get`.
    //
    using const_buffers_type =
        boost::asio::const_buffers_1;

    // Constructor.
    //
    // `m` holds the message we are sending, which will
    // always have the `file_body_stdc` as the body type.
    //
    template<bool isRequest, class Fields>
    reader(beast::http::message<isRequest, file_body_stdc, Fields> const& m,
        beast::error_code& ec);

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

//[example_http_file_body_stdc_4

// Here we just stash a reference to the path for later.
// Rather than dealing with messy constructor exceptions,
// we save the things that might fail for the call to `init`.
//
template<bool isRequest, class Fields>
file_body_stdc::reader::
reader(beast::http::message<isRequest, file_body_stdc, Fields> const& m,
        beast::error_code& ec)
    : file_(m.body.file_)
    , remain_(m.body.size())
{
    // The file must already be open
    BOOST_ASSERT(file_ != nullptr);

    // This is required by the error_code specification
    ec = {};
}

// This function is called repeatedly by the serializer to
// retrieve the buffers representing the body. Our strategy
// is to read into our buffer and return it until we have
// read through the whole file.
//
inline
auto
file_body_stdc::reader::
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

//]

//[example_http_file_body_stdc_5

class file_body_stdc::writer
{
    FILE* file_; // The file handle

public:
    // Constructor.
    //
    // This is called after the header is parsed and
    // indicates that a non-zero sized body may be present.
    // `m` holds the message we are receiving, which will
    // always have the `file_body_stdc` as the body type.
    //
    template<bool isRequest, class Fields>
    explicit
    writer(beast::http::message<isRequest, file_body_stdc, Fields>& m,
        boost::optional<std::uint64_t> const& content_length,
            beast::error_code& ec);

    // This function is called one or more times to store
    // buffer sequences corresponding to the incoming body.
    //
    template<class ConstBufferSequence>
    std::size_t
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
};

//]

//[example_http_file_body_stdc_6

// Just stash a reference to the path so we can open the file later.
template<bool isRequest, class Fields>
file_body_stdc::writer::
writer(beast::http::message<isRequest, file_body_stdc, Fields>& m,
    boost::optional<std::uint64_t> const& content_length,
        beast::error_code& ec)
    : file_(m.body.file_)
{
    // We don't do anything with this but a sophisticated
    // application might check available space on the device
    // to see if there is enough room to store the body.
    boost::ignore_unused(content_length);

    // The file must already be open for writing
    BOOST_ASSERT(file_ != nullptr);

    // This is required by the error_code specification
    ec = {};
}

// This will get called one or more times with body buffers
//
template<class ConstBufferSequence>
std::size_t
file_body_stdc::writer::
put(ConstBufferSequence const& buffers, beast::error_code& ec)
{
    // This function must return the total number of
    // bytes transferred from the input buffers.
    std::size_t bytes_transferred = 0;

    // Loop over all the buffers in the sequence,
    // and write each one to the file.
    for(boost::asio::const_buffer buffer : buffers)
    {
        // Write this buffer to the file
        bytes_transferred += fwrite(
            boost::asio::buffer_cast<void const*>(buffer), 1,
            boost::asio::buffer_size(buffer),
            file_);

        // Handle any errors
        if(ferror(file_))
        {
            // Convert the old-school `errno` into
            // an error code using the generic category.
            ec = beast::error_code{errno, beast::generic_category()};
            return bytes_transferred;
        }
    }

    // Indicate success
    ec = {};

    return bytes_transferred;
}

// Called after writing is done when there's no error.
inline
void
file_body_stdc::writer::
finish(beast::error_code& ec)
{
    // This has to be cleared before returning, to
    // indicate no error. The specification requires it.
    ec = {};
}

//]

} // http
} // beast

#endif
