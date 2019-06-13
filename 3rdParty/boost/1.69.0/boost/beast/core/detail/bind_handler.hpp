//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_BIND_HANDLER_HPP
#define BOOST_BEAST_DETAIL_BIND_HANDLER_HPP

#include <boost/beast/core/detail/integer_sequence.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/is_placeholder.hpp>
#include <functional>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

/*  Nullary handler that calls Handler with bound arguments.

    The bound handler provides the same io_context execution
    guarantees as the original handler.
*/
template<class Handler, class... Args>
class bound_handler
{
    // Can't friend partial specializations,
    // so we just friend the whole thing.
    template<class T, class Executor>
    friend struct boost::asio::associated_executor;

    using args_type = std::tuple<
        typename std::decay<Args>::type...>;

    Handler h_;
    args_type args_;

    template<class Arg, class Vals>
    static
    typename std::enable_if<
        std::is_placeholder<typename
            std::decay<Arg>::type>::value == 0 &&
        boost::is_placeholder<typename
            std::decay<Arg>::type>::value == 0,
        Arg&&>::type
    extract(Arg&& arg, Vals&& vals)
    {
        boost::ignore_unused(vals);
        return std::forward<Arg>(arg);
    }

    template<class Arg, class Vals>
    static
    typename std::enable_if<
        std::is_placeholder<typename
            std::decay<Arg>::type>::value != 0,
        typename std::tuple_element<
            std::is_placeholder<
                typename std::decay<Arg>::type>::value - 1,
        Vals>>::type::type&&
    extract(Arg&&, Vals&& vals)
    {
        return std::get<std::is_placeholder<
            typename std::decay<Arg>::type>::value - 1>(
                std::forward<Vals>(vals));
    }

    template<class Arg, class Vals>
    static
    typename std::enable_if<
        boost::is_placeholder<typename
            std::decay<Arg>::type>::value != 0,
        typename std::tuple_element<
            boost::is_placeholder<
                typename std::decay<Arg>::type>::value - 1,
        Vals>>::type::type&&
    extract(Arg&&, Vals&& vals)
    {
        return std::get<boost::is_placeholder<
            typename std::decay<Arg>::type>::value - 1>(
                std::forward<Vals>(vals));
    }

    template<
        class ArgsTuple,
        std::size_t... S>
    static
    void
    invoke(
        Handler& h,
        ArgsTuple& args,
        std::tuple<>&&,
        index_sequence<S...>)
    {
        boost::ignore_unused(args);
        h(std::get<S>(std::move(args))...);
    }

    template<
        class ArgsTuple,
        class ValsTuple,
        std::size_t... S>
    static
    void
    invoke(
        Handler& h,
        ArgsTuple& args,
        ValsTuple&& vals,
        index_sequence<S...>)
    {
        boost::ignore_unused(args);
        boost::ignore_unused(vals);
        h(extract(std::get<S>(std::move(args)),
            std::forward<ValsTuple>(vals))...);
    }

public:
    using result_type = void;

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    bound_handler(bound_handler&&) = default;
    bound_handler(bound_handler const&) = delete;

    template<class DeducedHandler>
    explicit
    bound_handler(
            DeducedHandler&& handler, Args&&... args)
        : h_(std::forward<DeducedHandler>(handler))
        , args_(std::forward<Args>(args)...)
    {
    }

    allocator_type
    get_allocator() const noexcept
    {
        return (boost::asio::get_associated_allocator)(h_);
    }

    friend
    bool
    asio_handler_is_continuation(bound_handler* h)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(h->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, bound_handler* h)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(h->h_));
    }

    template<class... Values>
    void
    operator()(Values&&... values)
    {
        invoke(h_, args_,
            std::forward_as_tuple(
                std::forward<Values>(values)...),
            index_sequence_for<Args...>());
    }

    template<class... Values>
    void
    operator()(Values&&... values) const
    {
        invoke(h_, args_,
            std::forward_as_tuple(
                std::forward<Values>(values)...),
            index_sequence_for<Args...>());
    }
};

} // detail
} // beast

namespace asio {
template<class Handler, class... Args, class Executor>
struct associated_executor<
    beast::detail::bound_handler<Handler, Args...>, Executor>
{
    using type = typename
        associated_executor<Handler, Executor>::type;

    static
    type
    get(beast::detail::bound_handler<Handler, Args...> const& h,
        Executor const& ex = Executor()) noexcept
    {
        return associated_executor<
            Handler, Executor>::get(h.h_, ex);
    }
};
} // asio

} // boost

namespace std {
template<class Handler, class... Args>
void
bind(boost::beast::detail::bound_handler<
    Handler, Args...>, ...) = delete;
} // std

#endif
