//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_UTF8_CHECKER_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_UTF8_CHECKER_HPP

#include <boost/beast/core/detail/polymorphic_buffer_sequence.hpp>
#include <boost/asio/buffer.hpp>

#include <cstdint>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

/** A UTF8 validator.

    This validator can be used to check if a buffer containing UTF8 text is
    valid. The write function may be called incrementally with segmented UTF8
    sequences. The finish function determines if all processed text is valid.
*/
class utf8_checker
{
    std::size_t need_ = 0;  // chars we need to finish the code point
    std::uint8_t* p_ = cp_; // current position in temp buffer
    std::uint8_t cp_[4];    // a temp buffer for the code point

public:
    /** Prepare to process text as valid utf8
    */
    BOOST_BEAST_DECL
    void
    reset();

    /** Check that all processed text is valid utf8
    */
    BOOST_BEAST_DECL
    bool
    finish();

    /** Check if text is valid UTF8

        @return `true` if the text is valid utf8 or false otherwise.
    */
    BOOST_BEAST_DECL
    bool
    write(std::uint8_t const* in, std::size_t size);

    /** Check if text is valid UTF8

        @return `true` if the text is valid utf8 or false otherwise.
    */
    inline
    bool
    write(beast::detail::polymorphic_const_buffer_sequence const& bs);
};


bool
utf8_checker::
write(beast::detail::polymorphic_const_buffer_sequence const& buffers)
{
    for(auto b : buffers)
        if(! write(static_cast<
            std::uint8_t const*>(b.data()),
                b.size()))
            return false;
    return true;
}


BOOST_BEAST_DECL
bool
check_utf8(char const* p, std::size_t n);

} // detail
} // websocket
} // beast
} // boost

#if BOOST_BEAST_HEADER_ONLY
#include <boost/beast/websocket/detail/utf8_checker.ipp>
#endif

#endif
