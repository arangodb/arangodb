//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_FIELDS_HPP
#define BOOST_BEAST_HTTP_FIELDS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/string_param.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace http {

/** A container for storing HTTP header fields.

    This container is designed to store the field value pairs that make
    up the fields and trailers in an HTTP message. Objects of this type
    are iterable, with each element holding the field name and field
    value.

    Field names are stored as-is, but comparisons are case-insensitive.
    The container behaves as a `std::multiset`; there will be a separate
    value for each occurrence of the same field name. When the container
    is iterated the fields are presented in the order of insertion, with
    fields having the same name following each other consecutively.

    Meets the requirements of @b Fields

    @tparam Allocator The allocator to use. This must meet the
    requirements of @b Allocator.
*/
template<class Allocator>
class basic_fields
{
    friend class fields_test; // for `header`

    static std::size_t constexpr max_static_buffer = 4096;

    using off_t = std::uint16_t;

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

    /// The type of element used to represent a field 
    class value_type
    {
        friend class basic_fields;

        boost::asio::const_buffer
        buffer() const;

        value_type(field name,
            string_view sname, string_view value);

        boost::intrusive::list_member_hook<
            boost::intrusive::link_mode<
                boost::intrusive::normal_link>>
                    list_hook_;
        boost::intrusive::set_member_hook<
            boost::intrusive::link_mode<
                boost::intrusive::normal_link>>
                    set_hook_;
        off_t off_;
        off_t len_;
        field f_;

    public:
        /// Constructor (deleted)
        value_type(value_type const&) = delete;

        /// Assignment (deleted)
        value_type& operator=(value_type const&) = delete;

        /// Returns the field enum, which can be @ref field::unknown
        field const
        name() const;

        /// Returns the field name as a string
        string_view const
        name_string() const;

        /// Returns the value of the field
        string_view const
        value() const;
    };

    /** A strictly less predicate for comparing keys, using a case-insensitive comparison.

        The case-comparison operation is defined only for low-ASCII characters.
    */
    struct key_compare : beast::iless
    {
        /// Returns `true` if lhs is less than rhs using a strict ordering
        template<class String>
        bool
        operator()(
            String const& lhs,
            value_type const& rhs) const noexcept
        {
            if(lhs.size() < rhs.name_string().size())
                return true;
            if(lhs.size() > rhs.name_string().size())
                return false;
            return iless::operator()(lhs, rhs.name_string());
        }

        /// Returns `true` if lhs is less than rhs using a strict ordering
        template<class String>
        bool
        operator()(
            value_type const& lhs,
            String const& rhs) const noexcept
        {
            if(lhs.name_string().size() < rhs.size())
                return true;
            if(lhs.name_string().size() > rhs.size())
                return false;
            return iless::operator()(lhs.name_string(), rhs);
        }

        /// Returns `true` if lhs is less than rhs using a strict ordering
        bool
        operator()(
            value_type const& lhs,
            value_type const& rhs) const noexcept
        {
            if(lhs.name_string().size() < rhs.name_string().size())
                return true;
            if(lhs.name_string().size() > rhs.name_string().size())
                return false;
            return iless::operator()(lhs.name_string(), rhs.name_string());
        }
    };

    /// The algorithm used to serialize the header
#if BOOST_BEAST_DOXYGEN
    using writer = implementation_defined;
#else
    class writer;
#endif

private:
    using list_t = typename boost::intrusive::make_list<
        value_type, boost::intrusive::member_hook<
            value_type, boost::intrusive::list_member_hook<
                boost::intrusive::link_mode<
                    boost::intrusive::normal_link>>,
                        &value_type::list_hook_>,
                            boost::intrusive::constant_time_size<
                                false>>::type;

    using set_t = typename boost::intrusive::make_multiset<
        value_type, boost::intrusive::member_hook<value_type,
            boost::intrusive::set_member_hook<
                boost::intrusive::link_mode<
                    boost::intrusive::normal_link>>,
                        &value_type::set_hook_>,
                            boost::intrusive::constant_time_size<true>,
                                boost::intrusive::compare<key_compare>>::type;


public:
    /// Destructor
    ~basic_fields();

    /// Constructor.
    basic_fields() = default;

    /** Constructor.

        @param alloc The allocator to use.
    */
    explicit
    basic_fields(Allocator const& alloc);

    /** Move constructor.

        The state of the moved-from object is
        as if constructed using the same allocator.
    */
    basic_fields(basic_fields&&);

    /** Move constructor.

        The state of the moved-from object is
        as if constructed using the same allocator.

        @param alloc The allocator to use.
    */
    basic_fields(basic_fields&&, Allocator const& alloc);

    /// Copy constructor.
    basic_fields(basic_fields const&);

    /** Copy constructor.

        @param alloc The allocator to use.
    */
    basic_fields(basic_fields const&, Allocator const& alloc);

    /// Copy constructor.
    template<class OtherAlloc>
    basic_fields(basic_fields<OtherAlloc> const&);

    /** Copy constructor.

        @param alloc The allocator to use.
    */
    template<class OtherAlloc>
    basic_fields(basic_fields<OtherAlloc> const&,
        Allocator const& alloc);

    /** Move assignment.

        The state of the moved-from object is
        as if constructed using the same allocator.
    */
    basic_fields& operator=(basic_fields&&);

    /// Copy assignment.
    basic_fields& operator=(basic_fields const&);

    /// Copy assignment.
    template<class OtherAlloc>
    basic_fields& operator=(basic_fields<OtherAlloc> const&);

public:
    /// A constant iterator to the field sequence.
#if BOOST_BEAST_DOXYGEN
    using const_iterator = implementation_defined;
#else
    using const_iterator = typename list_t::const_iterator;
#endif

    /// A constant iterator to the field sequence.
    using iterator = const_iterator;

    /// Return a copy of the allocator associated with the container.
    allocator_type
    get_allocator() const
    {
        return alloc_;
    }

    //--------------------------------------------------------------------------
    //
    // Element access
    //
    //--------------------------------------------------------------------------

    /** Returns the value for a field, or throws an exception.

        If more than one field with the specified name exists, the
        first field defined by insertion order is returned.

        @param name The name of the field.

        @return The field value.

        @throws std::out_of_range if the field is not found.
    */
    string_view const
    at(field name) const;

    /** Returns the value for a field, or throws an exception.

        If more than one field with the specified name exists, the
        first field defined by insertion order is returned.

        @param name The name of the field.

        @return The field value.

        @throws std::out_of_range if the field is not found.
    */
    string_view const
    at(string_view name) const;

    /** Returns the value for a field, or `""` if it does not exist.

        If more than one field with the specified name exists, the
        first field defined by insertion order is returned.

        @param name The name of the field.
    */
    string_view const
    operator[](field name) const;

    /** Returns the value for a case-insensitive matching header, or `""` if it does not exist.

        If more than one field with the specified name exists, the
        first field defined by insertion order is returned.

        @param name The name of the field.
    */
    string_view const
    operator[](string_view name) const;

    //--------------------------------------------------------------------------
    //
    // Iterators
    //
    //--------------------------------------------------------------------------

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

    //--------------------------------------------------------------------------
    //
    // Capacity
    //
    //--------------------------------------------------------------------------

private:
    // VFALCO Since the header and message derive from Fields,
    //        what does the expression m.empty() mean? Its confusing.
    bool
    empty() const
    {
        return list_.empty();
    }
public:

    //--------------------------------------------------------------------------
    //
    // Modifiers
    //
    //--------------------------------------------------------------------------

    /** Remove all fields from the container

        All references, pointers, or iterators referring to contained
        elements are invalidated. All past-the-end iterators are also
        invalidated.

        @par Postconditions:
        @code
            std::distance(this->begin(), this->end()) == 0
        @endcode
    */
    void
    clear();

    /** Insert a field.

        If one or more fields with the same name already exist,
        the new field will be inserted after the last field with
        the matching name, in serialization order.

        @param name The field name.

        @param value The value of the field, as a @ref string_param
    */
    void
    insert(field name, string_param const& value);

    /** Insert a field.

        If one or more fields with the same name already exist,
        the new field will be inserted after the last field with
        the matching name, in serialization order.

        @param name The field name.

        @param value The value of the field, as a @ref string_param
    */
    void
    insert(string_view name, string_param const& value);

    /** Insert a field.

        If one or more fields with the same name already exist,
        the new field will be inserted after the last field with
        the matching name, in serialization order.

        @param name The field name.

        @param name_string The literal text corresponding to the
        field name. If `name != field::unknown`, then this value
        must be equal to `to_string(name)` using a case-insensitive
        comparison, otherwise the behavior is undefined.

        @param value The value of the field, as a @ref string_param
    */
    void
    insert(field name, string_view name_string,
        string_param const& value);

    /** Set a field value, removing any other instances of that field.

        First removes any values with matching field names, then
        inserts the new field value.

        @param name The field name.

        @param value The value of the field, as a @ref string_param

        @return The field value.
    */
    void
    set(field name, string_param const& value);

    /** Set a field value, removing any other instances of that field.

        First removes any values with matching field names, then
        inserts the new field value.

        @param name The field name.

        @param value The value of the field, as a @ref string_param
    */
    void
    set(string_view name, string_param const& value);

    /** Remove a field.

        References and iterators to the erased elements are
        invalidated. Other references and iterators are not
        affected.

        @param pos An iterator to the element to remove.

        @return An iterator following the last removed element.
        If the iterator refers to the last element, the end()
        iterator is returned.
    */
    const_iterator
    erase(const_iterator pos);

    /** Remove all fields with the specified name.

        All fields with the same field name are erased from the
        container.
        References and iterators to the erased elements are
        invalidated. Other references and iterators are not
        affected.

        @param name The field name.

        @return The number of fields removed.
    */
    std::size_t
    erase(field name);

    /** Remove all fields with the specified name.

        All fields with the same field name are erased from the
        container.
        References and iterators to the erased elements are
        invalidated. Other references and iterators are not
        affected.

        @param name The field name.

        @return The number of fields removed.
    */
    std::size_t
    erase(string_view name);

    /** Return a buffer sequence representing the trailers.

        This function returns a buffer sequence holding the
        serialized representation of the trailer fields promised
        in the Accept field. Before calling this function the
        Accept field must contain the exact trailer fields
        desired. Each field must also exist.
    */


    /// Swap this container with another
    void
    swap(basic_fields& other);

    /// Swap two field containers
    template<class Alloc>
    friend
    void
    swap(basic_fields<Alloc>& lhs, basic_fields<Alloc>& rhs);

    //--------------------------------------------------------------------------
    //
    // Lookup
    //
    //--------------------------------------------------------------------------

    /** Return the number of fields with the specified name.

        @param name The field name.
    */
    std::size_t
    count(field name) const;

    /** Return the number of fields with the specified name.

        @param name The field name.
    */
    std::size_t
    count(string_view name) const;

    /** Returns an iterator to the case-insensitive matching field.

        If more than one field with the specified name exists, the
        first field defined by insertion order is returned.

        @param name The field name.

        @return An iterator to the matching field, or `end()` if
        no match was found.
    */
    const_iterator
    find(field name) const;

    /** Returns an iterator to the case-insensitive matching field name.

        If more than one field with the specified name exists, the
        first field defined by insertion order is returned.

        @param name The field name.

        @return An iterator to the matching field, or `end()` if
        no match was found.
    */
    const_iterator
    find(string_view name) const;

    /** Returns a range of iterators to the fields with the specified name.

        @param name The field name.

        @return A range of iterators to fields with the same name,
        otherwise an empty range.
    */
    std::pair<const_iterator, const_iterator>
    equal_range(field name) const;

    /** Returns a range of iterators to the fields with the specified name.

        @param name The field name.

        @return A range of iterators to fields with the same name,
        otherwise an empty range.
    */
    std::pair<const_iterator, const_iterator>
    equal_range(string_view name) const;

    //--------------------------------------------------------------------------
    //
    // Observers
    //
    //--------------------------------------------------------------------------

    /// Returns a copy of the key comparison function
    key_compare
    key_comp() const
    {
        return key_compare{};
    }

protected:
    /** Returns the request-method string.

        @note Only called for requests.
    */
    string_view
    get_method_impl() const;

    /** Returns the request-target string.

        @note Only called for requests.
    */
    string_view
    get_target_impl() const;

    /** Returns the response reason-phrase string.

        @note Only called for responses.
    */
    string_view
    get_reason_impl() const;

    /** Returns the chunked Transfer-Encoding setting
    */
    bool
    get_chunked_impl() const;

    /** Returns the keep-alive setting
    */
    bool
    get_keep_alive_impl(unsigned version) const;

    /** Returns `true` if the Content-Length field is present.
    */
    bool
    has_content_length_impl() const;

    /** Set or clear the method string.

        @note Only called for requests.
    */
    void
    set_method_impl(string_view s);

    /** Set or clear the target string.

        @note Only called for requests.
    */
    void
    set_target_impl(string_view s);

    /** Set or clear the reason string.

        @note Only called for responses.
    */
    void
    set_reason_impl(string_view s);

    /** Adjusts the chunked Transfer-Encoding value
    */
    void
    set_chunked_impl(bool value);

    /** Sets or clears the Content-Length field
    */
    void
    set_content_length_impl(
        boost::optional<std::uint64_t> const& value);

    /** Adjusts the Connection field
    */
    void
    set_keep_alive_impl(
        unsigned version, bool keep_alive);

private:
    template<class OtherAlloc>
    friend class basic_fields;

    using base_alloc_type = typename
        beast::detail::allocator_traits<Allocator>::
            template rebind_alloc<value_type>;

    using alloc_traits =
        beast::detail::allocator_traits<base_alloc_type>;

    using size_type = typename
        beast::detail::allocator_traits<Allocator>::size_type;

    value_type&
    new_element(field name,
        string_view sname, string_view value);

    void
    delete_element(value_type& e);

    void
    set_element(value_type& e);

    void
    realloc_string(string_view& dest, string_view s);

    void
    realloc_target(
        string_view& dest, string_view s);

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

    base_alloc_type alloc_;
    set_t set_;
    list_t list_;
    string_view method_;
    string_view target_or_reason_;
};

/// A typical HTTP header fields container
using fields = basic_fields<std::allocator<char>>;

} // http
} // beast
} // boost

#include <boost/beast/http/impl/fields.ipp>

#endif
