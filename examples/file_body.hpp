//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_FILE_BODY_H_INCLUDED
#define BEAST_EXAMPLE_FILE_BODY_H_INCLUDED

#include <beast/core/error.hpp>
#include <beast/http/message.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdint>

namespace beast {
namespace http {

struct file_body
{
    using value_type = std::string;

    /// Returns the content length of the body in a message.
    template<bool isRequest, class Fields>
    static
    std::uint64_t
    size(
        message<isRequest, file_body, Fields> const& m)
    {
        return boost::filesystem::file_size(m.body.c_str());
    }

    class reader
    {
        std::uint64_t size_ = 0;
        std::uint64_t offset_ = 0;
        std::string const& path_;
        FILE* file_ = nullptr;
        char buf_[4096];

    public:
        using is_deferred = std::true_type;

        using const_buffers_type =
            boost::asio::const_buffers_1;

        reader(reader&&) = default;
        reader(reader const&) = delete;
        reader& operator=(reader const&) = delete;

        template<bool isRequest, class Fields>
        reader(message<isRequest,
                file_body, Fields> const& m)
            : path_(m.body)
        {
        }

        ~reader()
        {
            if(file_)
                fclose(file_);
        }

        void
        init(error_code& ec)
        {
            file_ = fopen(path_.c_str(), "rb");
            if(! file_)
                ec = error_code{errno, system_category()};
            else
                size_ = boost::filesystem::file_size(path_);
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            auto const amount = std::min<std::uint64_t>(size_ - offset_, sizeof(buf_));
            auto const nread = fread(buf_, 1, amount, file_);
            if(ferror(file_))
            {
                ec = error_code(errno, system_category());
                return boost::none;
            }
            BOOST_ASSERT(nread != 0);
            offset_ += nread;
            return {{const_buffers_type{buf_, nread}, offset_ >= size_}};
        }
    };
};

} // http
} // beast

#endif
