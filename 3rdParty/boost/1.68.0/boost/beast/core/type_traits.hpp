//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TYPE_TRAITS_HPP
#define BOOST_BEAST_TYPE_TRAITS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/file_base.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <type_traits>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------
//
// Handler concepts
//
//------------------------------------------------------------------------------

/** Determine if `T` meets the requirements of @b CompletionHandler.

    This trait checks whether a type meets the requirements for a completion
    handler, and is also callable with the specified signature.
    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example

    Use with `static_assert`:

    @code
    struct handler
    {
        void operator()(error_code&);
    };

    static_assert(is_completion_handler<handler, void(error_code&)>::value,
        "Not a completion handler");
    @endcode
*/
template<class T, class Signature>
#if BOOST_BEAST_DOXYGEN
using is_completion_handler = std::integral_constant<bool, ...>;
#else
using is_completion_handler = std::integral_constant<bool,
    std::is_move_constructible<typename std::decay<T>::type>::value &&
    detail::is_invocable<T, Signature>::value>;
#endif

//------------------------------------------------------------------------------
//
// Stream concepts
//
//------------------------------------------------------------------------------

/** Determine if `T` has the `get_executor` member function.

    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` has the member
    function with the correct signature, else type will be `std::false_type`. 

    @par Example

    Use with tag dispatching:

    @code
    template<class T>
    void maybe_hello(T& t, std::true_type)
    {
        boost::asio::post(
            t.get_executor(),
            []
            {
                std::cout << "Hello, world!" << std::endl;
            });
    }

    template<class T>
    void maybe_hello(T&, std::false_type)
    {
        // T does not have get_executor
    }

    template<class T>
    void maybe_hello(T& t)
    {
        maybe_hello(t, has_get_executor<T>{});
    }
    @endcode

    Use with `static_assert`:

    @code
    struct stream
    {
        using executor_type = boost::asio::io_context::executor_type;
        executor_type get_executor() noexcept;
    };

    static_assert(has_get_executor<stream>::value, "Missing get_executor member");
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct has_get_executor : std::integral_constant<bool, ...>{};
#else
template<class T, class = void>
struct has_get_executor : std::false_type {};

template<class T>
struct has_get_executor<T, beast::detail::void_t<decltype(
        std::declval<T&>().get_executor())>> : std::true_type {};
#endif

/** Alias for `T::lowest_layer_type` if it exists, else `T`

    This will be a type alias for `T::lowest_layer_type`
    if it exists, else it will be an alias for `T`.

    @par Example

    Declaring a wrapper:

    @code
    template<class Stream>
    struct stream_wrapper
    {
        using next_layer_type = typename std::remove_reference<Stream>::type;
        using lowest_layer_type = get_lowest_layer<stream_type>;
    };
    @endcode

    Defining a metafunction:

    @code
    /// Alias for `std::true_type` if `T` wraps another stream
    template<class T>
    using is_stream_wrapper : std::integral_constant<bool,
        ! std::is_same<T, get_lowest_layer<T>>::value> {};
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct get_lowest_layer;
#else
template<class T>
using get_lowest_layer = typename detail::get_lowest_layer_helper<T>::type;
#endif

/** Determine if `T` meets the requirements of @b AsyncReadStream.

    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example
    
    Use with `static_assert`:
    
    @code
    template<class AsyncReadStream>
    void f(AsyncReadStream& stream)
    {
        static_assert(is_async_read_stream<AsyncReadStream>::value,
            "AsyncReadStream requirements not met");
    ...
    @endcode
    
    Use with `std::enable_if` (SFINAE):
    
    @code
        template<class AsyncReadStream>
        typename std::enable_if<is_async_read_stream<AsyncReadStream>::value>::type
        f(AsyncReadStream& stream);
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_async_read_stream : std::integral_constant<bool, ...>{};
#else
template<class T, class = void>
struct is_async_read_stream : std::false_type {};

template<class T>
struct is_async_read_stream<T, detail::void_t<decltype(
    std::declval<T>().async_read_some(
        std::declval<detail::MutableBufferSequence>(),
        std::declval<detail::ReadHandler>())
            )>> : std::integral_constant<bool,
    has_get_executor<T>::value
        > {};
#endif

/** Determine if `T` meets the requirements of @b AsyncWriteStream.

    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example

    Use with `static_assert`:

    @code
    template<class AsyncWriteStream>
    void f(AsyncWriteStream& stream)
    {
        static_assert(is_async_write_stream<AsyncWriteStream>::value,
            "AsyncWriteStream requirements not met");
    ...
    @endcode

    Use with `std::enable_if` (SFINAE):

    @code
    template<class AsyncWriteStream>
    typename std::enable_if<is_async_write_stream<AsyncWriteStream>::value>::type
    f(AsyncWriteStream& stream);
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_async_write_stream : std::integral_constant<bool, ...>{};
#else
template<class T, class = void>
struct is_async_write_stream : std::false_type {};

template<class T>
struct is_async_write_stream<T, detail::void_t<decltype(
    std::declval<T>().async_write_some(
        std::declval<detail::ConstBufferSequence>(),
        std::declval<detail::WriteHandler>())
            )>> : std::integral_constant<bool,
    has_get_executor<T>::value
        > {};
#endif

/** Determine if `T` meets the requirements of @b SyncReadStream.

    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example

    Use with `static_assert`:

    @code
    template<class SyncReadStream>
    void f(SyncReadStream& stream)
    {
        static_assert(is_sync_read_stream<SyncReadStream>::value,
            "SyncReadStream requirements not met");
    ...
    @endcode

    Use with `std::enable_if` (SFINAE):

    @code
    template<class SyncReadStream>
    typename std::enable_if<is_sync_read_stream<SyncReadStream>::value>::type
    f(SyncReadStream& stream);
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_sync_read_stream : std::integral_constant<bool, ...>{};
#else
template<class T, class = void>
struct is_sync_read_stream : std::false_type {};

template<class T>
struct is_sync_read_stream<T, detail::void_t<decltype(
    std::declval<std::size_t&>() = std::declval<T>().read_some(
        std::declval<detail::MutableBufferSequence>()),
    std::declval<std::size_t&>() = std::declval<T>().read_some(
        std::declval<detail::MutableBufferSequence>(),
        std::declval<boost::system::error_code&>())
            )>> : std::true_type {};
#endif

/** Determine if `T` meets the requirements of @b SyncWriteStream.

    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example

    Use with `static_assert`:

    @code
    template<class SyncReadStream>
    void f(SyncReadStream& stream)
    {
        static_assert(is_sync_read_stream<SyncReadStream>::value,
            "SyncReadStream requirements not met");
    ...
    @endcode

    Use with `std::enable_if` (SFINAE):

    @code
    template<class SyncReadStream>
    typename std::enable_if<is_sync_read_stream<SyncReadStream>::value>::type
    f(SyncReadStream& stream);
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_sync_write_stream : std::integral_constant<bool, ...>{};
#else
template<class T, class = void>
struct is_sync_write_stream : std::false_type {};

template<class T>
struct is_sync_write_stream<T, detail::void_t<decltype(
    std::declval<std::size_t&>() = std::declval<T&>().write_some(
        std::declval<detail::ConstBufferSequence>()),
    std::declval<std::size_t&>() = std::declval<T&>().write_some(
        std::declval<detail::ConstBufferSequence>(),
        std::declval<boost::system::error_code&>())
            )>> : std::true_type {};
#endif

/** Determine if `T` meets the requirements of @b AsyncStream.

    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example

    Use with `static_assert`:

    @code
    template<class AsyncStream>
    void f(AsyncStream& stream)
    {
        static_assert(is_async_stream<AsyncStream>::value,
            "AsyncStream requirements not met");
    ...
    @endcode

    Use with `std::enable_if` (SFINAE):

    @code
    template<class AsyncStream>
    typename std::enable_if<is_async_stream<AsyncStream>::value>::type
    f(AsyncStream& stream);
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_async_stream : std::integral_constant<bool, ...>{};
#else
template<class T>
using is_async_stream = std::integral_constant<bool,
    is_async_read_stream<T>::value && is_async_write_stream<T>::value>;
#endif

/** Determine if `T` meets the requirements of @b SyncStream.

    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example

    Use with `static_assert`:

    @code
    template<class SyncStream>
    void f(SyncStream& stream)
    {
        static_assert(is_sync_stream<SyncStream>::value,
            "SyncStream requirements not met");
    ...
    @endcode

    Use with `std::enable_if` (SFINAE):

    @code
    template<class SyncStream>
    typename std::enable_if<is_sync_stream<SyncStream>::value>::type
    f(SyncStream& stream);
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_sync_stream : std::integral_constant<bool, ...>{};
#else
template<class T>
using is_sync_stream = std::integral_constant<bool,
    is_sync_read_stream<T>::value && is_sync_write_stream<T>::value>;
#endif

//------------------------------------------------------------------------------
//
// File concepts
//
//------------------------------------------------------------------------------

/** Determine if `T` meets the requirements of @b File.

    Metafunctions are used to perform compile time checking of template
    types. This type will be `std::true_type` if `T` meets the requirements,
    else the type will be `std::false_type`. 

    @par Example

    Use with `static_assert`:

    @code
    template<class File>
    void f(File& file)
    {
        static_assert(is_file<File>::value,
            "File requirements not met");
    ...
    @endcode

    Use with `std::enable_if` (SFINAE):

    @code
    template<class File>
    typename std::enable_if<is_file<File>::value>::type
    f(File& file);
    @endcode
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
struct is_file : std::integral_constant<bool, ...>{};
#else
template<class T, class = void>
struct is_file : std::false_type {};

template<class T>
struct is_file<T, detail::void_t<decltype(
    std::declval<bool&>() = std::declval<T const&>().is_open(),
    std::declval<T&>().close(std::declval<error_code&>()),
    std::declval<T&>().open(
        std::declval<char const*>(),
        std::declval<file_mode>(),
        std::declval<error_code&>()),
    std::declval<std::uint64_t&>() = std::declval<T&>().size(
        std::declval<error_code&>()),
    std::declval<std::uint64_t&>() = std::declval<T&>().pos(
        std::declval<error_code&>()),
    std::declval<T&>().seek(
        std::declval<std::uint64_t>(),
        std::declval<error_code&>()),
    std::declval<std::size_t&>() = std::declval<T&>().read(
        std::declval<void*>(),
        std::declval<std::size_t>(),
        std::declval<error_code&>()),
    std::declval<std::size_t&>() = std::declval<T&>().write(
        std::declval<void const*>(),
        std::declval<std::size_t>(),
        std::declval<error_code&>())
            )>> : std::integral_constant<bool,
    std::is_default_constructible<T>::value &&
    std::is_destructible<T>::value
        > {};
#endif

} // beast
} // boost

#endif
