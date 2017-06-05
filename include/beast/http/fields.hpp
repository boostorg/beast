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

    @note Meets the requirements of @b FieldSequence.
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

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

    /// A constant iterator to the field sequence.
    class const_iterator;

    /// A constant iterator to the field sequence.
    using iterator = const_iterator;

    /// Default constructor.
    basic_fields() = default;

    /// Destructor
    ~basic_fields();

    /** Construct the fields.

        @param alloc The allocator to use.
    */
    explicit
    basic_fields(Allocator const& alloc);

    /** Move constructor.

        The moved-from object becomes an empty field sequence.

        @param other The object to move from.
    */
    basic_fields(basic_fields&& other);

    /** Move assignment.

        The moved-from object becomes an empty field sequence.

        @param other The object to move from.
    */
    basic_fields& operator=(basic_fields&& other);

    /// Copy constructor.
    basic_fields(basic_fields const&);

    /// Copy assignment.
    basic_fields& operator=(basic_fields const&);

    /// Copy constructor.
    template<class OtherAlloc>
    basic_fields(basic_fields<OtherAlloc> const&);

    /// Copy assignment.
    template<class OtherAlloc>
    basic_fields& operator=(basic_fields<OtherAlloc> const&);

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

    /** Remove a field.

        If more than one field with the specified name exists, all
        matching fields will be removed.

        @param name The name of the field(s) to remove.

        @return The number of fields removed.
    */
    std::size_t
    erase(string_view name);

    /** Insert a field value.

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

#if BEAST_DOXYGEN
private:
#endif

    string_view
    method_impl() const
    {
        return (*this)[":method"];
    }

    void
    method_impl(string_view s)
    {
        if(s.empty())
            this->erase(":method");
        else
            this->replace(":method", s);
    }

    string_view
    target_impl() const
    {
        return (*this)[":target"];
    }

    void
    target_impl(string_view s)
    {
        if(s.empty())
            this->erase(":target");
        else
            return this->replace(":target", s);
    }

    string_view
    reason_impl() const
    {
        return (*this)[":reason"];
    }

    void
    reason_impl(string_view s)
    {
        if(s.empty())
            this->erase(":reason");
        else
            this->replace(":reason", s);
    }

private:
    struct element
        : boost::intrusive::set_base_hook <
            boost::intrusive::link_mode <
                boost::intrusive::normal_link>>
        , boost::intrusive::list_base_hook <
            boost::intrusive::link_mode <
                boost::intrusive::normal_link>>
    {
        element(
            string_view name, string_view value)
        {
            char* p = reinterpret_cast<char*>(this + 1);
            data.first = p;
            data.off =
                static_cast<off_t>(name.size() + 2);
            data.len =
                static_cast<off_t>(value.size());
            std::memcpy(p, name.data(), name.size());
            p[data.off-2] = ':';
            p[data.off-1] = ' ';
            std::memcpy(
                p + data.off, value.data(), value.size());
            p[data.off + data.len] = '\r';
            p[data.off + data.len + 1] = '\n';
        }

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

    void
    delete_all();

    void
    move_assign(basic_fields&, std::false_type);

    void
    move_assign(basic_fields&, std::true_type);

    void
    copy_assign(basic_fields const&, std::false_type);

    void
    copy_assign(basic_fields const&, std::true_type);

    template<class FieldSequence>
    void
    copy_from(FieldSequence const& fs)
    {
        for(auto const& e : fs)
            insert(e.name(), e.value());
    }

    element&
    new_element(string_view name, string_view value);

    void
    delete_element(element& e);

    set_t set_;
    list_t list_;
    alloc_type alloc_;
};

/// A typical HTTP header fields container
using fields =
    basic_fields<std::allocator<char>>;

} // http
} // beast

#include <beast/http/impl/fields.ipp>

#endif
