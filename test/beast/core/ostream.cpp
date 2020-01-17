//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/ostream.hpp>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <ostream>

namespace boost {
namespace beast {

class ostream_test : public beast::unit_test::suite
{
public:
    void
    testOstream()
    {
        string_view const s = "0123456789abcdef";
        BEAST_EXPECT(s.size() == 16);

        // overflow
        {
            flat_static_buffer<16> b;
            ostream(b) << s;
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        }

        // max_size
        {
            flat_static_buffer<16> b;
            auto os = ostream(b);
            os << s;
            os << '*';
            BEAST_EXPECT(os.bad());
        }

        // max_size (exception
        {
            flat_static_buffer<16> b;
            auto os = ostream(b);
            os.exceptions(os.badbit);
            os << s;
            try
            {
                os << '*';
                fail("missing exception", __FILE__, __LINE__);
            }
            catch(std::ios_base::failure const&)
            {
                pass();
            }
            catch(...)
            {
                fail("wrong exception", __FILE__, __LINE__);
            }
        }
    }

    template<class MakeBuffer>
    void
    testOstreamWithV1orV2(MakeBuffer make_buffer)
    {
        std::string target;
        string_view const s = "0123456789abcdef";
        BEAST_EXPECT(s.size() == 16);

        // overflow
        {
            target.clear();
            ostream(make_buffer(target, 16)) << s;
            BEAST_EXPECT(target == s);
        }

        // max_size
        {
            target.clear();
            flat_static_buffer<16> b;
            auto os = ostream(make_buffer(target, 16));
            os << s;
            os << '*';
            BEAST_EXPECT(os.bad());
            BEAST_EXPECT(target == s);
        }

        // max_size (exception
        {
            target.clear();
            auto os = ostream(make_buffer(target, 16));
            os.exceptions(os.badbit);
            os << s;
            try
            {
                os << '*';
                fail("missing exception", __FILE__, __LINE__);
            }
            catch(std::ios_base::failure const&)
            {
                pass();
            }
            catch(...)
            {
                fail("wrong exception", __FILE__, __LINE__);
            }
        }
    }

    // copied directly from boost asio 1.72 and edited as-if NO_DYNAMIC_BUFFER_V1
    struct v1_dynamic_string_buffer
    {
        typedef net::const_buffer const_buffers_type;
        typedef net::mutable_buffer mutable_buffers_type;
        explicit v1_dynamic_string_buffer(std::string& s,
                                          std::size_t maximum_size =
                                          (std::numeric_limits<std::size_t>::max)()) BOOST_ASIO_NOEXCEPT
            : string_(s),
            size_((std::numeric_limits<std::size_t>::max)()),
            max_size_(maximum_size)
        {
        }

        std::size_t size() const BOOST_ASIO_NOEXCEPT
        {
            if (size_ != (std::numeric_limits<std::size_t>::max)())
                return size_;
            return (std::min)(string_.size(), max_size());
        }

        std::size_t max_size() const BOOST_ASIO_NOEXCEPT
        {
            return max_size_;
        }

        std::size_t capacity() const BOOST_ASIO_NOEXCEPT
        {
            return (std::min)(string_.capacity(), max_size());
        }

        const_buffers_type data() const BOOST_ASIO_NOEXCEPT
        {
            return const_buffers_type(boost::asio::buffer(string_, size_));
        }

        mutable_buffers_type prepare(std::size_t n)
        {
            if (size() > max_size() || max_size() - size() < n)
            {
                std::length_error ex("dynamic_string_buffer too long");
                boost::asio::detail::throw_exception(ex);
            }

            if (size_ == (std::numeric_limits<std::size_t>::max)())
                size_ = string_.size(); // Enable v1 behaviour.

            string_.resize(size_ + n);

            return boost::asio::buffer(boost::asio::buffer(string_) + size_, n);
        }

        void commit(std::size_t n)
        {
            size_ += (std::min)(n, string_.size() - size_);
            string_.resize(size_);
        }

        void consume(std::size_t n)
        {
            if (size_ != (std::numeric_limits<std::size_t>::max)())
            {
                std::size_t consume_length = (std::min)(n, size_);
                string_.erase(0, consume_length);
                size_ -= consume_length;
                return;
            }
            string_.erase(0, n);
        }

    private:
        std::string& string_;
        std::size_t size_;
        const std::size_t max_size_;
    };

    void
    run() override
    {
        testOstreamWithV1orV2([](std::string& s, std::size_t max) { return v1_dynamic_string_buffer(s, max); });
        testOstreamWithV1orV2([](std::string& s, std::size_t max) { return net::dynamic_buffer(s, max); });
        testOstream();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,ostream);

} // beast
} // boost
