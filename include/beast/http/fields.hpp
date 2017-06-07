//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_FIELDS_HPP
#define BEAST_HTTP_FIELDS_HPP

#include <beast/config.hpp>
#include <beast/core/string_view.hpp>
#include <beast/core/detail/ci_char_traits.hpp>
#include <beast/http/connection.hpp>
#include <beast/http/field.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

/** A container for storing HTTP header fields.

    This container is designed to store the field value pairs that make
    up the fields and trailers in an HTTP message. Objects of this type
    are iterable, with each element holding the field name and field
    value.

    Field names are stored as-is, but comparisons are case-insensitive.
    When the container is iterated, the fields are presented in the order
    of insertion. For fields with the same name, the container behaves
    as a `std::multiset`; there will be a separate value for each occurrence
    of the field name.

    @note Meets the requirements of @b Fields
*/
template<class Allocator>
class basic_fields
{
private:
    using off_t = std::uint16_t;

public:
    /** The value type of the field sequence.

        Meets the requirements of @b Field.
    */
    struct value_type
    {
        string_view
        name() const
        {
            return {first, off - 2u};
        }

        string_view
        value() const
        {
            return {first + off, len};
        }

        char const* first;
        off_t off;
        off_t len;
    };

protected:
    //
    // These are for `header`
    //
    friend class fields_test;

    /// Destructor
    ~basic_fields();

    /// Default constructor.
    basic_fields() = default;

    /** Constructor.

        @param alloc The allocator to use.
    */
    explicit
    basic_fields(Allocator const& alloc);

    /** Move constructor.

        The moved-from object behaves as if by call to @ref clear.
    */
    basic_fields(basic_fields&&);

    /// Copy constructor.
    basic_fields(basic_fields const&);

    /** Copy constructor.

        @param alloc The allocator to use.
    */
    basic_fields(basic_fields const&, Allocator const& alloc);

    /// Copy constructor.
#if BEAST_DOXYGEN
    template<class OtherAlloc>
#else
    template<class OtherAlloc, class =
        typename std::enable_if<! std::is_same<OtherAlloc,
            Allocator>::value>::type>
#endif
    basic_fields(basic_fields<OtherAlloc> const&);

    /** Copy constructor.

        @param alloc The allocator to use.
    */
#if BEAST_DOXYGEN
    template<class OtherAlloc>
#else
    template<class OtherAlloc, class =
        typename std::enable_if<! std::is_same<OtherAlloc,
            Allocator>::value>::type>
#endif
    basic_fields(basic_fields<OtherAlloc> const&,
        Allocator const& alloc);

    /** Move assignment.

        The moved-from object behaves as if by call to @ref clear.
    */
    basic_fields& operator=(basic_fields&&);

    /// Copy assignment.
    basic_fields& operator=(basic_fields const&);

    /// Copy assignment.
#if BEAST_DOXYGEN
    template<class OtherAlloc>
#else
    template<class OtherAlloc, class =
        typename std::enable_if<! std::is_same<OtherAlloc,
            Allocator>::value>::type>
#endif
    basic_fields& operator=(basic_fields<OtherAlloc> const&);

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

    /// A constant iterator to the field sequence.
    class const_iterator;

    /// A constant iterator to the field sequence.
    using iterator = const_iterator;

    /// Return a copy of the allocator associated with the container.
    allocator_type
    get_allocator() const
    {
        return typename std::allocator_traits<
            Allocator>::template rebind_alloc<
                element>(alloc_);
    }

    /// Return a const iterator to the beginning of the field sequence.
    const_iterator
    begin() const
    {
        return list_.cbegin();
    }

    /// Return a const iterator to the end of the field sequence.
    const_iterator
    end() const
    {
        return list_.cend();
    }

    /// Return a const iterator to the beginning of the field sequence.
    const_iterator
    cbegin() const
    {
        return list_.cbegin();
    }

    /// Return a const iterator to the end of the field sequence.
    const_iterator
    cend() const
    {
        return list_.cend();
    }

    /// Return `true` if the specified field exists.
    bool
    exists(string_view name) const
    {
        return set_.find(name, less{}) != set_.end();
    }

    /// Return the number of values for the specified field.
    std::size_t
    count(string_view name) const;

    /** Returns an iterator to the case-insensitive matching field name.

        If more than one field with the specified name exists, the
        first field defined by insertion order is returned.
    */
    iterator
    find(string_view name) const;

    /** Returns the value for a case-insensitive matching header, or `""`.

        If more than one field with the specified name exists, the
        first field defined by insertion order is returned.
    */
    string_view const
    operator[](string_view name) const;

    /// Clear the contents of the basic_fields.
    void
    clear() noexcept;

    /** Remove zero or more known fields.

        If more than one field with the specified name exists, all
        matching fields will be removed.

        @param f The known field constant.

        @return The number of fields removed.
    */
    std::size_t
    erase(field f);

    /** Remove zero or more fields by name.

        If more than one field with the specified name exists, all
        matching fields will be removed.

        @param name The name of the field(s) to remove.

        @return The number of fields removed.
    */
    std::size_t
    erase(string_view name);

    /** Insert a value for a known field.

        If a field with the same name already exists, the
        existing field is untouched and a new field value pair
        is inserted into the container.

        @param f The known field constant.

        @param value A string holding the value of the field.
    */
    void
    insert(field f, string_view value);

    /** Insert a value for a field by name.

        If a field with the same name already exists, the
        existing field is untouched and a new field value pair
        is inserted into the container.

        @param name The name of the field.

        @param value A string holding the value of the field.
    */
    void
    insert(string_view name, string_view value);

    /** Insert a field value.

        If a field with the same name already exists, the
        existing field is untouched and a new field value pair
        is inserted into the container.

        @param name The name of the field

        @param value The value of the field. The object will be
        converted to a string using `boost::lexical_cast`.
    */
    template<class T>
    typename std::enable_if<
        ! std::is_constructible<string_view, T>::value>::type
    insert(string_view name, T const& value)
    {
        // VFALCO This should use a static buffer, see lexical_cast doc
        insert(name, boost::lexical_cast<std::string>(value));
    }

    /** Replace a field value.

        First removes any values with matching field names, then
        inserts the new field value.

        @param name The name of the field.

        @param value A string holding the value of the field.
    */
    void
    replace(string_view name, string_view value);

    /** Replace a field value.

        First removes any values with matching field names, then
        inserts the new field value.

        @param name The name of the field

        @param value The value of the field. The object will be
        converted to a string using `boost::lexical_cast`.
    */
    template<class T>
    typename std::enable_if<
        ! std::is_constructible<string_view, T>::value>::type
    replace(string_view name, T const& value)
    {
        // VFALCO This should use a static buffer, see lexical_cast doc
        replace(name,
            boost::lexical_cast<std::string>(value));
    }

    /// Swap two field containers
    template<class Alloc>
    friend
    void
    swap(basic_fields<Alloc>& lhs, basic_fields<Alloc>& rhs);

    /// The algorithm used to serialize the header
    class reader;

protected:
    /// Returns `true` if the value for Connection has "close" in the list.
    bool has_close_impl() const;

    /// Returns `true` if "chunked" is the last Transfer-Encoding
    bool has_chunked_impl() const;

    /// Returns `true` if the Content-Length field is present
    bool has_content_length_impl() const;

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

    //--------------------------------------------------------------------------
    //
    // for container
    //

    /** Set the Content-Length field to the specified value.

        @note This is called by the @ref header implementation.
    */
    void content_length_impl(std::uint64_t n);

    /** Add close to the Connection field.

        @note This is called by the @ref header implementation.
    */
    void connection_impl(close_t);

    /** Add keep-alive to the Connection field.

        @note This is called by the @ref header implementation.
    */
    void connection_impl(keep_alive_t);

    /** Add upgrade to the Connection field.

        @note This is called by the @ref header implementation.
    */
    void connection_impl(upgrade_t);

    /** Add chunked to the Transfer-Encoding field.

        @note This is called by the @ref header implementation.
    */
    void set_chunked_impl(bool v);

private:
    class element
        : public boost::intrusive::set_base_hook <
            boost::intrusive::link_mode <
                boost::intrusive::normal_link>>
        , public boost::intrusive::list_base_hook <
            boost::intrusive::link_mode <
                boost::intrusive::normal_link>>
    {
        off_t off_;
        off_t len_;

    public:
        element(string_view name, string_view value);

        string_view
        name() const;

        string_view
        value() const;

        boost::asio::const_buffer
        buffer() const;

        value_type data;
    };

    struct less : private beast::detail::ci_less
    {
        template<class String>
        bool
        operator()(String const& lhs, element const& rhs) const
        {
            return ci_less::operator()(lhs, rhs.data.name());
        }

        template<class String>
        bool
        operator()(element const& lhs, String const& rhs) const
        {
            return ci_less::operator()(lhs.data.name(), rhs);
        }

        bool
        operator()(element const& lhs, element const& rhs) const
        {
            return ci_less::operator()(
                lhs.data.name(), rhs.data.name());
        }
    };

    using alloc_type = typename
        std::allocator_traits<Allocator>::
            template rebind_alloc<element>;

    using alloc_traits =
        std::allocator_traits<alloc_type>;

    using size_type =
        typename std::allocator_traits<Allocator>::size_type;

    using list_t = typename boost::intrusive::make_list<
        element, boost::intrusive::constant_time_size<
            false>>::type;

    using set_t = typename boost::intrusive::make_multiset<
        element, boost::intrusive::constant_time_size<
            true>, boost::intrusive::compare<less>>::type;

    element&
    new_element(string_view name, string_view value);

    void
    delete_element(element& e);

    void
    realloc_string(string_view& dest, string_view s);

    template<class OtherAlloc>
    void
    copy_all(basic_fields<OtherAlloc> const&);

    void
    clear_all();

    void
    delete_list();

    void
    move_assign(basic_fields&, std::true_type);

    void
    move_assign(basic_fields&, std::false_type);

    void
    copy_assign(basic_fields const&, std::true_type);

    void
    copy_assign(basic_fields const&, std::false_type);

    void
    swap(basic_fields& other, std::true_type);

    void
    swap(basic_fields& other, std::false_type);

    set_t set_;
    list_t list_;
    string_view method_;
    string_view target_or_reason_;
    alloc_type alloc_;
};

/// A typical HTTP header fields container
using fields =
    basic_fields<std::allocator<char>>;

} // http
} // beast

#include <beast/http/impl/fields.ipp>

#endif
