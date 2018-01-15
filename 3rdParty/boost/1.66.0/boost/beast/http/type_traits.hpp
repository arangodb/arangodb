//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_TYPE_TRAITS_HPP
#define BOOST_BEAST_HTTP_TYPE_TRAITS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/http/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace http {

template<bool, class, class>
struct message;

/** Determine if `T` meets the requirements of @b Body.

    This metafunction is equivalent to `std::true_type`
    if `T` has a nested type named `value_type`.

    @tparam T The body type to test.

    @par Example
    @code
    template<bool isRequest, class Body, class Fields>
    void check_body(message<isRequest, Body, Fields> const&)
    {
        static_assert(is_body<Body>::value,
            "Body requirements not met");
    }
    @endcode
*/
template<class T>
#if BOOST_BEAST_DOXYGEN
struct is_body : std::integral_constant<bool, ...>{};
#else
using is_body = detail::has_value_type<T>;
#endif

/** Determine if a @b Body type has a reader.

    This metafunction is equivalent to `std::true_type` if:

    @li `T` has a nested type named `reader`

    @li The nested type meets the requirements of @b BodyWriter.

    @tparam T The body type to test.

    @par Example
    @code
    template<bool isRequest, class Body, class Fields>
    void check_can_serialize(message<isRequest, Body, Fields> const&)
    {
        static_assert(is_body_writer<Body>::value,
            "Cannot serialize Body, no reader");
    }
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_body_writer : std::integral_constant<bool, ...> {};
#else
template<class T, class = void>
struct is_body_writer : std::false_type {};

template<class T>
struct is_body_writer<T, beast::detail::void_t<
    typename T::writer,
    typename T::writer::const_buffers_type,
        decltype(
    std::declval<typename T::writer&>().init(std::declval<error_code&>()),
    std::declval<boost::optional<std::pair<
            typename T::writer::const_buffers_type, bool>>&>() =
            std::declval<typename T::writer>().get(std::declval<error_code&>()),
        (void)0)>> : std::integral_constant<bool,
    boost::asio::is_const_buffer_sequence<
        typename T::writer::const_buffers_type>::value &&
    std::is_constructible<typename T::writer,
        message<true, T, detail::fields_model>&>::value &&
    std::is_constructible<typename T::writer,
        message<false, T, detail::fields_model>&>::value
    > {};
#endif

/** Determine if a @b Body type has a reader.

    This metafunction is equivalent to `std::true_type` if:

    @li `T` has a nested type named `reader`

    @li The nested type meets the requirements of @b BodyReader.

    @tparam T The body type to test.

    @par Example
    @code
    template<bool isRequest, class Body, class Fields>
    void check_can_parse(message<isRequest, Body, Fields>&)
    {
        static_assert(is_body_reader<Body>::value,
            "Cannot parse Body, no reader");
    }
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_body_reader : std::integral_constant<bool, ...> {};
#else
template<class T, class = void>
struct is_body_reader : std::false_type {};

template<class T>
struct is_body_reader<T, beast::detail::void_t<decltype(
    std::declval<typename T::reader&>().init(
        boost::optional<std::uint64_t>(),
        std::declval<error_code&>()),
    std::declval<std::size_t&>() =
        std::declval<typename T::reader&>().put(
            std::declval<boost::asio::const_buffer>(),
            std::declval<error_code&>()),
    std::declval<typename T::reader&>().finish(
        std::declval<error_code&>()),
    (void)0)>> : std::integral_constant<bool,
        std::is_constructible<typename T::reader,
            message<true, T, detail::fields_model>&>::value &&
        std::is_constructible<typename T::reader,
            message<false, T, detail::fields_model>&>::value
            >
{
};
#endif

/** Determine if `T` meets the requirements of @b Fields

    @tparam T The body type to test.

    @par Example

    Use with `static_assert`:

    @code
    template<bool isRequest, class Body, class Fields>
    void f(message<isRequest, Body, Fields> const&)
    {
        static_assert(is_fields<Fields>::value,
            "Fields requirements not met");
    ...
    @endcode

    Use with `std::enable_if` (SFINAE):

    @code
    template<bool isRequest, class Body, class Fields>
    typename std::enable_if<is_fields<Fields>::value>::type
    f(message<isRequest, Body, Fields> const&);
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_fields : std::integral_constant<bool, ...> {};
#else
template<class T>
using is_fields = typename detail::is_fields_helper<T>::type;
#endif

} // http
} // beast
} // boost

#endif
