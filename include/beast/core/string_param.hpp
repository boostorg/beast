//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_STRING_PARAM_HPP
#define BEAST_STRING_PARAM_HPP

#include <beast/config.hpp>
#include <beast/core/string_view.hpp>
#include <beast/core/detail/static_ostream.hpp>
#include <beast/core/detail/type_traits.hpp>

namespace beast {

/** A function parameter which efficiently converts to string.

    This is used as a function parameter type to allow callers
    notational convenience: objects other than strings may be
    passed in contexts where a string is expected. The conversion
    to string is made using `operator<<` to a non-dynamically
    allocated static buffer if possible, else to a `std::string`
    on overflow. 
*/
class string_param
{
    template<class T, class = void>
    struct is_streamable : std::false_type{};

#if ! BEAST_DOXYGEN
    template<class T>
    struct is_streamable<T, detail::void_t<decltype(
        std::declval<std::ostream&>() << std::declval<T const&>()
            )>> : std::true_type {};
#endif

    template<class T>
    void
    assign(T const& t, std::true_type)
    {
        sv_ = t;
    }

    template<class T>
    void
    assign(T const& t, std::false_type)
    {
        os_ << t;
        os_.flush();
        sv_ = os_.str();
    }

    detail::static_ostream os_;
    string_view sv_;

public:
    /// Copy constructor (disallowed)
    string_param(string_param const&) = delete;

    /// Copy assignment (disallowed)
    string_param& operator=(string_param const&) = delete;

    /** Constructor

        This function constructs a string from the specified
        parameter.
        
        If @ref string_view is constructible from `T` then
        the complexity is O(1), and no dynamic allocation
        takes place.

        Otherwise, the argument is converted to a string
        as if by a call to `boost::lexical_cast<std::string>(t)`.
        An attempt is made to store this string in a reasonably
        sized buffer inside the class, to avoid dynamic allocation.

        If the argument could not be converted given the space
        of the class buffer, a final attempt is made to convert
        the argument to a `std::string`. This attempt may cause
        a memory allocation.

        @param t The argument to convert to string.

        @note This function participates in overload resolution
        only if `T` is convertible to @ref string_view, or if
        `operator<<(std::ostream&, T)` is defined.
    */
#if BEAST_DOXYGEN
    template<class T>
#else
    template<class T, class = typename std::enable_if<
        std::is_convertible<T, string_view>::value ||
        is_streamable<T>::value
            >::type>
#endif
    string_param(T const& t);

    /// Conversion to @ref string_view for the converted argument.
    string_view const
    str() const
    {
        return sv_;
    }

    /// Conversion to @ref string_view operator
    operator string_view const() const
    {
        return sv_;
    }
};

#if ! BEAST_DOXYGEN
template<class T, class>
string_param::
string_param(T const& t)
{
    assign(t, std::is_convertible<T, string_view>{});
}
#endif

} // beast

#endif
