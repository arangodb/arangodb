#ifndef BOOST_MP11_ALGORITHM_HPP_INCLUDED
#define BOOST_MP11_ALGORITHM_HPP_INCLUDED

//  Copyright 2015-2017 Peter Dimov.
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/list.hpp>
#include <boost/mp11/set.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/detail/mp_count.hpp>
#include <boost/mp11/detail/mp_plus.hpp>
#include <boost/mp11/detail/mp_map_find.hpp>
#include <boost/mp11/detail/mp_with_index.hpp>
#include <boost/mp11/detail/mp_fold.hpp>
#include <boost/mp11/detail/mp_min_element.hpp>
#include <boost/mp11/detail/config.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <type_traits>
#include <utility>

namespace boost
{
namespace mp11
{

// mp_transform<F, L...>
namespace detail
{

template<template<class...> class F, class... L> struct mp_transform_impl
{
};

template<template<class...> class F, template<class...> class L, class... T> struct mp_transform_impl<F, L<T...>>
{
#if BOOST_WORKAROUND( BOOST_MSVC, < 1920 )

    template<class... U> struct f { using type = F<U...>; };

    using type = L<typename f<T>::type...>;

#else

    using type = L<F<T>...>;

#endif
};

template<template<class...> class F, template<class...> class L1, class... T1, template<class...> class L2, class... T2> struct mp_transform_impl<F, L1<T1...>, L2<T2...>>
{
#if BOOST_WORKAROUND( BOOST_MSVC, < 1920 )

    template<class... U> struct f { using type = F<U...>; };

    using type = L1<typename f<T1, T2>::type...>;

#else

    using type = L1<F<T1,T2>...>;

#endif
};

template<template<class...> class F, template<class...> class L1, class... T1, template<class...> class L2, class... T2, template<class...> class L3, class... T3> struct mp_transform_impl<F, L1<T1...>, L2<T2...>, L3<T3...>>
{
#if BOOST_WORKAROUND( BOOST_MSVC, < 1920 )

    template<class... U> struct f { using type = F<U...>; };

    using type = L1<typename f<T1, T2, T3>::type...>;

#else

    using type = L1<F<T1,T2,T3>...>;

#endif
};

#if BOOST_WORKAROUND( BOOST_MSVC, == 1900 ) || BOOST_WORKAROUND( BOOST_GCC, < 40800 )

template<class... L> using mp_same_size_1 = mp_same<mp_size<L>...>;
template<class... L> struct mp_same_size_2: mp_defer<mp_same_size_1, L...> {};

#endif

struct list_size_mismatch
{
};

#if BOOST_WORKAROUND( BOOST_CUDA_VERSION, >= 9000000 && BOOST_CUDA_VERSION < 10000000 )

template<template<class...> class F, class... L> struct mp_transform_cuda_workaround
{
    using type = mp_if<mp_same<mp_size<L>...>, detail::mp_transform_impl<F, L...>, detail::list_size_mismatch>;
};

#endif

} // namespace detail

#if BOOST_WORKAROUND( BOOST_MSVC, == 1900 ) || BOOST_WORKAROUND( BOOST_GCC, < 40800 )

template<template<class...> class F, class... L> using mp_transform = typename mp_if<typename detail::mp_same_size_2<L...>::type, detail::mp_transform_impl<F, L...>, detail::list_size_mismatch>::type;

#else

#if BOOST_WORKAROUND( BOOST_CUDA_VERSION, >= 9000000 && BOOST_CUDA_VERSION < 10000000 )

template<template<class...> class F, class... L> using mp_transform = typename detail::mp_transform_cuda_workaround< F, L...>::type::type;

#else

template<template<class...> class F, class... L> using mp_transform = typename mp_if<mp_same<mp_size<L>...>, detail::mp_transform_impl<F, L...>, detail::list_size_mismatch>::type;

#endif

#endif

template<class Q, class... L> using mp_transform_q = mp_transform<Q::template fn, L...>;

namespace detail
{

template<template<class...> class F, template<class...> class L1, class... T1, template<class...> class L2, class... T2, template<class...> class L3, class... T3, template<class...> class L4, class... T4, class... L> struct mp_transform_impl<F, L1<T1...>, L2<T2...>, L3<T3...>, L4<T4...>, L...>
{
    using A1 = L1<mp_list<T1, T2, T3, T4>...>;

    template<class V, class T> using _f = mp_transform<mp_push_back, V, T>;

    using A2 = mp_fold<mp_list<L...>, A1, _f>;

    template<class T> using _g = mp_apply<F, T>;

    using type = mp_transform<_g, A2>;
};

} // namespace detail

// mp_transform_if<P, F, L...>
namespace detail
{

template<template<class...> class P, template<class...> class F, class... L> struct mp_transform_if_impl
{
    // the stupid quote-unquote dance avoids "pack expansion used as argument for non-pack parameter of alias template"

    using Qp = mp_quote<P>;
    using Qf = mp_quote<F>;

#if BOOST_WORKAROUND( BOOST_MSVC, < 1920 )

    template<class... U> struct _f_ { using type = mp_eval_if_q<mp_not<mp_invoke<Qp, U...>>, mp_first<mp_list<U...>>, Qf, U...>; };
    template<class... U> using _f = typename _f_<U...>::type;

#else

    template<class... U> using _f = mp_eval_if_q<mp_not<mp_invoke<Qp, U...>>, mp_first<mp_list<U...>>, Qf, U...>;

#endif

    using type = mp_transform<_f, L...>;
};

} // namespace detail

template<template<class...> class P, template<class...> class F, class... L> using mp_transform_if = typename detail::mp_transform_if_impl<P, F, L...>::type;
template<class Qp, class Qf, class... L> using mp_transform_if_q = typename detail::mp_transform_if_impl<Qp::template fn, Qf::template fn, L...>::type;

// mp_fill<L, V>
namespace detail
{

template<class L, class V> struct mp_fill_impl;

template<template<class...> class L, class... T, class V> struct mp_fill_impl<L<T...>, V>
{
#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, <= 1900 )

    template<class...> struct _f { using type = V; };
    using type = L<typename _f<T>::type...>;

#else

    template<class...> using _f = V;
    using type = L<_f<T>...>;

#endif
};

} // namespace detail

template<class L, class V> using mp_fill = typename detail::mp_fill_impl<L, V>::type;

// mp_contains<L, V>
template<class L, class V> using mp_contains = mp_to_bool<mp_count<L, V>>;

// mp_repeat(_c)<L, N>
namespace detail
{

template<class L, std::size_t N> struct mp_repeat_c_impl
{
    using _l1 = typename mp_repeat_c_impl<L, N/2>::type;
    using _l2 = typename mp_repeat_c_impl<L, N%2>::type;

    using type = mp_append<_l1, _l1, _l2>;
};

template<class L> struct mp_repeat_c_impl<L, 0>
{
    using type = mp_clear<L>;
};

template<class L> struct mp_repeat_c_impl<L, 1>
{
    using type = L;
};

} // namespace detail

template<class L, std::size_t N> using mp_repeat_c = typename detail::mp_repeat_c_impl<L, N>::type;
template<class L, class N> using mp_repeat = typename detail::mp_repeat_c_impl<L, std::size_t{ N::value }>::type;

// mp_product<F, L...>
namespace detail
{

template<template<class...> class F, class P, class... L> struct mp_product_impl_2;

template<template<class...> class F, class P> struct mp_product_impl_2<F, P>
{
    using type = mp_list<mp_rename<P, F>>;
};

template<template<class...> class F, class P, template<class...> class L1, class... T1, class... L> struct mp_product_impl_2<F, P, L1<T1...>, L...>
{
    using type = mp_append<typename mp_product_impl_2<F, mp_push_back<P, T1>, L...>::type...>;
};

template<template<class...> class F, class... L> struct mp_product_impl;

template<template<class...> class F, class L1, class... L> struct mp_product_impl<F, L1, L...>
{
    using type = mp_assign<L1, typename mp_product_impl_2<F, mp_list<>, L1, L...>::type>;
};

} // namespace detail

template<template<class...> class F, class... L> using mp_product = typename detail::mp_product_impl<F, L...>::type;
template<class Q, class... L> using mp_product_q = typename detail::mp_product_impl<Q::template fn, L...>::type;

// mp_drop(_c)<L, N>
namespace detail
{

template<class L, class L2> struct mp_drop_impl;

template<template<class...> class L, class... T, template<class...> class L2, class... U> struct mp_drop_impl<L<T...>, L2<U...>>
{
    template<class... W> static mp_identity<L<W...>> f( U*..., mp_identity<W>*... );

    using R = decltype( f( (mp_identity<T>*)0 ... ) );

    using type = typename R::type;
};

} // namespace detail

template<class L, std::size_t N> using mp_drop_c = typename detail::mp_drop_impl<L, mp_repeat_c<mp_list<void>, N>>::type;

template<class L, class N> using mp_drop = typename detail::mp_drop_impl<L, mp_repeat<mp_list<void>, N>>::type;

// mp_from_sequence<S>
namespace detail
{

template<class S> struct mp_from_sequence_impl;

template<template<class T, T... I> class S, class U, U... J> struct mp_from_sequence_impl<S<U, J...>>
{
    using type = mp_list<std::integral_constant<U, J>...>;
};

} // namespace detail

template<class S> using mp_from_sequence = typename detail::mp_from_sequence_impl<S>::type;

// mp_iota(_c)<N>
template<std::size_t N> using mp_iota_c = mp_from_sequence<make_index_sequence<N>>;
template<class N> using mp_iota = mp_from_sequence<make_integer_sequence<typename std::remove_const<decltype(N::value)>::type, N::value>>;

// mp_at(_c)<L, I>
namespace detail
{

template<class L, std::size_t I> struct mp_at_c_impl;

#if defined(BOOST_MP11_HAS_TYPE_PACK_ELEMENT)

template<template<class...> class L, class... T, std::size_t I> struct mp_at_c_impl<L<T...>, I>
{
    using type = __type_pack_element<I, T...>;
};

#else

template<class L, std::size_t I> struct mp_at_c_impl
{
    using _map = mp_transform<mp_list, mp_iota<mp_size<L> >, L>;
    using type = mp_second<mp_map_find<_map, mp_size_t<I> > >;
};

#endif

#if BOOST_WORKAROUND( BOOST_CUDA_VERSION, >= 9000000 && BOOST_CUDA_VERSION < 10000000 )

template<class L, std::size_t I> struct mp_at_c_cuda_workaround
{
    using type = mp_if_c<(I < mp_size<L>::value), detail::mp_at_c_impl<L, I>, void>;
};

#endif

} // namespace detail

#if BOOST_WORKAROUND( BOOST_CUDA_VERSION, >= 9000000 && BOOST_CUDA_VERSION < 10000000 )

template<class L, std::size_t I> using mp_at_c = typename detail::mp_at_c_cuda_workaround< L, I >::type::type;

#else

template<class L, std::size_t I> using mp_at_c = typename mp_if_c<(I < mp_size<L>::value), detail::mp_at_c_impl<L, I>, void>::type;

#endif

template<class L, class I> using mp_at = mp_at_c<L, std::size_t{ I::value }>;

// mp_take(_c)<L, N>
namespace detail
{

template<class L, std::size_t N, class E = void> struct mp_take_c_impl;

template<template<class...> class L, class... T> struct mp_take_c_impl<L<T...>, 0>
{
    using type = L<>;
};

template<template<class...> class L, class T1, class... T> struct mp_take_c_impl<L<T1, T...>, 1>
{
    using type = L<T1>;
};

template<template<class...> class L, class T1, class T2, class... T> struct mp_take_c_impl<L<T1, T2, T...>, 2>
{
    using type = L<T1, T2>;
};

template<template<class...> class L, class T1, class T2, class T3, class... T> struct mp_take_c_impl<L<T1, T2, T3, T...>, 3>
{
    using type = L<T1, T2, T3>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class... T> struct mp_take_c_impl<L<T1, T2, T3, T4, T...>, 4>
{
    using type = L<T1, T2, T3, T4>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class... T, std::size_t N> struct mp_take_c_impl<L<T1, T2, T3, T4, T5, T...>, N, typename std::enable_if<N >= 5>::type>
{
    using type = mp_append<L<T1, T2, T3, T4, T5>, typename mp_take_c_impl<L<T...>, N-5>::type>;
};

} // namespace detail

template<class L, std::size_t N> using mp_take_c = typename detail::mp_take_c_impl<L, N>::type;
template<class L, class N> using mp_take = typename detail::mp_take_c_impl<L, std::size_t{ N::value }>::type;

// mp_replace<L, V, W>
namespace detail
{

template<class L, class V, class W> struct mp_replace_impl;

template<template<class...> class L, class... T, class V, class W> struct mp_replace_impl<L<T...>, V, W>
{
#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, <= 1800 )
    template<class A> struct _f { using type = mp_if<std::is_same<A, V>, W, A>; };
    using type = L<typename _f<T>::type...>;
#else
    template<class A> using _f = mp_if<std::is_same<A, V>, W, A>;
    using type = L<_f<T>...>;
#endif
};

} // namespace detail

template<class L, class V, class W> using mp_replace = typename detail::mp_replace_impl<L, V, W>::type;

// mp_replace_if<L, P, W>
namespace detail
{

template<class L, template<class...> class P, class W> struct mp_replace_if_impl;

template<template<class...> class L, class... T, template<class...> class P, class W> struct mp_replace_if_impl<L<T...>, P, W>
{
#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, < 1920 )
    template<class U> struct _f { using type = mp_if<P<U>, W, U>; };
    using type = L<typename _f<T>::type...>;
#else
    template<class U> using _f = mp_if<P<U>, W, U>;
    using type = L<_f<T>...>;
#endif
};

} // namespace detail

template<class L, template<class...> class P, class W> using mp_replace_if = typename detail::mp_replace_if_impl<L, P, W>::type;
template<class L, class Q, class W> using mp_replace_if_q = mp_replace_if<L, Q::template fn, W>;

// mp_copy_if<L, P>
namespace detail
{

template<class L, template<class...> class P> struct mp_copy_if_impl;

template<template<class...> class L, class... T, template<class...> class P> struct mp_copy_if_impl<L<T...>, P>
{
#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, < 1920 )
    template<class U> struct _f { using type = mp_if<P<U>, mp_list<U>, mp_list<>>; };
    using type = mp_append<L<>, typename _f<T>::type...>;
#else
    template<class U> using _f = mp_if<P<U>, mp_list<U>, mp_list<>>;
    using type = mp_append<L<>, _f<T>...>;
#endif
};

} // namespace detail

template<class L, template<class...> class P> using mp_copy_if = typename detail::mp_copy_if_impl<L, P>::type;
template<class L, class Q> using mp_copy_if_q = mp_copy_if<L, Q::template fn>;

// mp_remove<L, V>
namespace detail
{

template<class L, class V> struct mp_remove_impl;

template<template<class...> class L, class... T, class V> struct mp_remove_impl<L<T...>, V>
{
#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, < 1920 )
    template<class U> struct _f { using type = mp_if<std::is_same<U, V>, mp_list<>, mp_list<U>>; };
    using type = mp_append<L<>, typename _f<T>::type...>;
#else
    template<class U> using _f = mp_if<std::is_same<U, V>, mp_list<>, mp_list<U>>;
    using type = mp_append<L<>, _f<T>...>;
#endif
};

} // namespace detail

template<class L, class V> using mp_remove = typename detail::mp_remove_impl<L, V>::type;

// mp_remove_if<L, P>
namespace detail
{

template<class L, template<class...> class P> struct mp_remove_if_impl;

template<template<class...> class L, class... T, template<class...> class P> struct mp_remove_if_impl<L<T...>, P>
{
#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, < 1920 )
    template<class U> struct _f { using type = mp_if<P<U>, mp_list<>, mp_list<U>>; };
    using type = mp_append<L<>, typename _f<T>::type...>;
#else
    template<class U> using _f = mp_if<P<U>, mp_list<>, mp_list<U>>;
    using type = mp_append<L<>, _f<T>...>;
#endif
};

} // namespace detail

template<class L, template<class...> class P> using mp_remove_if = typename detail::mp_remove_if_impl<L, P>::type;
template<class L, class Q> using mp_remove_if_q = mp_remove_if<L, Q::template fn>;

// mp_partition<L, P>
namespace detail
{

template<class L, template<class...> class P> struct mp_partition_impl;

template<template<class...> class L, class... T, template<class...> class P> struct mp_partition_impl<L<T...>, P>
{
    using type = L<mp_copy_if<L<T...>, P>, mp_remove_if<L<T...>, P>>;
};

} // namespace detail

template<class L, template<class...> class P> using mp_partition = typename detail::mp_partition_impl<L, P>::type;
template<class L, class Q> using mp_partition_q = mp_partition<L, Q::template fn>;

// mp_sort<L, P>
namespace detail
{

template<class L, template<class...> class P> struct mp_sort_impl;

#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, <= 1800 )

template<template<class...> class L, class... T, template<class...> class P> struct mp_sort_impl<L<T...>, P>
{
    static_assert( sizeof...(T) == 0, "T... must be empty" );
    using type = L<>;
};

#else

template<template<class...> class L, template<class...> class P> struct mp_sort_impl<L<>, P>
{
    using type = L<>;
};

#endif

template<template<class...> class L, class T1, template<class...> class P> struct mp_sort_impl<L<T1>, P>
{
    using type = L<T1>;
};

template<template<class...> class L, class T1, class... T, template<class...> class P> struct mp_sort_impl<L<T1, T...>, P>
{
    template<class U> using F = P<U, T1>;

    using part = mp_partition<L<T...>, F>;

    using S1 = typename mp_sort_impl<mp_first<part>, P>::type;
    using S2 = typename mp_sort_impl<mp_second<part>, P>::type;

    using type = mp_append<mp_push_back<S1, T1>, S2>;
};

} // namespace detail

template<class L, template<class...> class P> using mp_sort = typename detail::mp_sort_impl<L, P>::type;
template<class L, class Q> using mp_sort_q = mp_sort<L, Q::template fn>;

// mp_nth_element(_c)<L, I, P>
namespace detail
{

template<class L, std::size_t I, template<class...> class P> struct mp_nth_element_impl;

template<template<class...> class L, class T1, std::size_t I, template<class...> class P> struct mp_nth_element_impl<L<T1>, I, P>
{
    static_assert( I == 0, "mp_nth_element index out of range" );
    using type = T1;
};

template<template<class...> class L, class T1, class... T, std::size_t I, template<class...> class P> struct mp_nth_element_impl<L<T1, T...>, I, P>
{
    static_assert( I < 1 + sizeof...(T), "mp_nth_element index out of range" );

    template<class U> using F = P<U, T1>;

    using part = mp_partition<L<T...>, F>;

    using L1 = mp_first<part>;
    static std::size_t const N1 = mp_size<L1>::value;

    using L2 = mp_second<part>;

    #if BOOST_WORKAROUND( BOOST_CUDA_VERSION, >= 9000000 && BOOST_CUDA_VERSION < 10000000 )

    struct detail
    {
        struct mp_nth_element_impl_cuda_workaround
        {
            using type = mp_cond<

                mp_bool<(I < N1)>, mp_nth_element_impl<L1, I, P>,
                mp_bool<(I == N1)>, mp_identity<T1>,
                mp_true, mp_nth_element_impl<L2, I - N1 - 1, P>

            >;
        };
    };
    using type = typename detail::mp_nth_element_impl_cuda_workaround::type::type;

    #else

    using type = typename mp_cond<

        mp_bool<(I < N1)>, mp_nth_element_impl<L1, I, P>,
        mp_bool<(I == N1)>, mp_identity<T1>,
        mp_true, mp_nth_element_impl<L2, I - N1 - 1, P>

    >::type;

    #endif
};

} // namespace detail

template<class L, std::size_t I, template<class...> class P> using mp_nth_element_c = typename detail::mp_nth_element_impl<L, I, P>::type;
template<class L, class I, template<class...> class P> using mp_nth_element = typename detail::mp_nth_element_impl<L, std::size_t{ I::value }, P>::type;
template<class L, class I, class Q> using mp_nth_element_q = mp_nth_element<L, I, Q::template fn>;

// mp_find<L, V>
namespace detail
{

template<class L, class V> struct mp_find_impl;

#if defined( BOOST_CLANG ) && defined( BOOST_MP11_HAS_FOLD_EXPRESSIONS )

struct mp_index_holder
{
    std::size_t i_;
    bool f_;
};

constexpr inline mp_index_holder operator+( mp_index_holder const & v, bool f )
{
    if( v.f_ )
    {
        return v;
    }
    else if( f )
    {
        return { v.i_, true };
    }
    else
    {
        return { v.i_ + 1, false };
    }
}

template<template<class...> class L, class... T, class V> struct mp_find_impl<L<T...>, V>
{
    static constexpr mp_index_holder _v{ 0, false };
    using type = mp_size_t< (_v + ... + std::is_same<T, V>::value).i_ >;
};

#elif !defined( BOOST_MP11_NO_CONSTEXPR )

template<template<class...> class L, class V> struct mp_find_impl<L<>, V>
{
    using type = mp_size_t<0>;
};

#if !defined( BOOST_NO_CXX14_CONSTEXPR )

constexpr std::size_t cx_find_index( bool const * first, bool const * last )
{
    std::size_t m = 0;

    while( first != last && !*first )
    {
        ++m;
        ++first;
    }

    return m;
}

#else

constexpr std::size_t cx_find_index( bool const * first, bool const * last )
{
    return first == last || *first? 0: 1 + cx_find_index( first + 1, last );
}

#endif

template<template<class...> class L, class... T, class V> struct mp_find_impl<L<T...>, V>
{
    static constexpr bool _v[] = { std::is_same<T, V>::value... };
    using type = mp_size_t< cx_find_index( _v, _v + sizeof...(T) ) >;
};

#else

#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, <= 1800 )

template<template<class...> class L, class... T, class V> struct mp_find_impl<L<T...>, V>
{
    static_assert( sizeof...(T) == 0, "T... must be empty" );
    using type = mp_size_t<0>;
};

#else

template<template<class...> class L, class V> struct mp_find_impl<L<>, V>
{
    using type = mp_size_t<0>;
};

#endif

template<template<class...> class L, class... T, class V> struct mp_find_impl<L<V, T...>, V>
{
    using type = mp_size_t<0>;
};

template<template<class...> class L, class T1, class... T, class V> struct mp_find_impl<L<T1, T...>, V>
{
    using _r = typename mp_find_impl<mp_list<T...>, V>::type;
    using type = mp_size_t<1 + _r::value>;
};

#endif

} // namespace detail

template<class L, class V> using mp_find = typename detail::mp_find_impl<L, V>::type;

// mp_find_if<L, P>
namespace detail
{

template<class L, template<class...> class P> struct mp_find_if_impl;

#if defined( BOOST_CLANG ) && defined( BOOST_MP11_HAS_FOLD_EXPRESSIONS )

template<template<class...> class L, class... T, template<class...> class P> struct mp_find_if_impl<L<T...>, P>
{
    static constexpr mp_index_holder _v{ 0, false };
    using type = mp_size_t< (_v + ... + P<T>::value).i_ >;
};

#elif !defined( BOOST_MP11_NO_CONSTEXPR )

template<template<class...> class L, template<class...> class P> struct mp_find_if_impl<L<>, P>
{
    using type = mp_size_t<0>;
};

template<template<class...> class L, class... T, template<class...> class P> struct mp_find_if_impl<L<T...>, P>
{
    static constexpr bool _v[] = { P<T>::value... };
    using type = mp_size_t< cx_find_index( _v, _v + sizeof...(T) ) >;
};

#else

#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, <= 1800 )

template<template<class...> class L, class... T, template<class...> class P> struct mp_find_if_impl<L<T...>, P>
{
    static_assert( sizeof...(T) == 0, "T... must be empty" );
    using type = mp_size_t<0>;
};

#else

template<template<class...> class L, template<class...> class P> struct mp_find_if_impl<L<>, P>
{
    using type = mp_size_t<0>;
};

#endif

template<class L, template<class...> class P> struct mp_find_if_impl_2
{
    using _r = typename mp_find_if_impl<L, P>::type;
    using type = mp_size_t<1 + _r::value>;
};

template<template<class...> class L, class T1, class... T, template<class...> class P> struct mp_find_if_impl<L<T1, T...>, P>
{
    using type = typename mp_if<P<T1>, mp_identity<mp_size_t<0>>, mp_find_if_impl_2<mp_list<T...>, P>>::type;
};

#endif

} // namespace detail

template<class L, template<class...> class P> using mp_find_if = typename detail::mp_find_if_impl<L, P>::type;
template<class L, class Q> using mp_find_if_q = mp_find_if<L, Q::template fn>;

// mp_reverse<L>
namespace detail
{

template<class L> struct mp_reverse_impl;

#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, <= 1800 )

template<template<class...> class L, class... T> struct mp_reverse_impl<L<T...>>
{
    static_assert( sizeof...(T) == 0, "T... must be empty" );
    using type = L<>;
};

#else

template<template<class...> class L> struct mp_reverse_impl<L<>>
{
    using type = L<>;
};

#endif

template<template<class...> class L, class T1> struct mp_reverse_impl<L<T1>>
{
    using type = L<T1>;
};

template<template<class...> class L, class T1, class T2> struct mp_reverse_impl<L<T1, T2>>
{
    using type = L<T2, T1>;
};

template<template<class...> class L, class T1, class T2, class T3> struct mp_reverse_impl<L<T1, T2, T3>>
{
    using type = L<T3, T2, T1>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4> struct mp_reverse_impl<L<T1, T2, T3, T4>>
{
    using type = L<T4, T3, T2, T1>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5> struct mp_reverse_impl<L<T1, T2, T3, T4, T5>>
{
    using type = L<T5, T4, T3, T2, T1>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class T6> struct mp_reverse_impl<L<T1, T2, T3, T4, T5, T6>>
{
    using type = L<T6, T5, T4, T3, T2, T1>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class T6, class T7> struct mp_reverse_impl<L<T1, T2, T3, T4, T5, T6, T7>>
{
    using type = L<T7, T6, T5, T4, T3, T2, T1>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8> struct mp_reverse_impl<L<T1, T2, T3, T4, T5, T6, T7, T8>>
{
    using type = L<T8, T7, T6, T5, T4, T3, T2, T1>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9> struct mp_reverse_impl<L<T1, T2, T3, T4, T5, T6, T7, T8, T9>>
{
    using type = L<T9, T8, T7, T6, T5, T4, T3, T2, T1>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class... T> struct mp_reverse_impl<L<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T...>>
{
    using type = mp_push_back<typename mp_reverse_impl<L<T...>>::type, T10, T9, T8, T7, T6, T5, T4, T3, T2, T1>;
};

} // namespace detail

template<class L> using mp_reverse = typename detail::mp_reverse_impl<L>::type;

// mp_fold<L, V, F>
//   in detail/mp_fold.hpp

// mp_reverse_fold<L, V, F>
namespace detail
{

template<class L, class V, template<class...> class F> struct mp_reverse_fold_impl;

#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, <= 1800 )

template<template<class...> class L, class... T, class V, template<class...> class F> struct mp_reverse_fold_impl<L<T...>, V, F>
{
    static_assert( sizeof...(T) == 0, "T... must be empty" );
    using type = V;
};

#else

template<template<class...> class L, class V, template<class...> class F> struct mp_reverse_fold_impl<L<>, V, F>
{
    using type = V;
};

#endif

template<template<class...> class L, class T1, class... T, class V, template<class...> class F> struct mp_reverse_fold_impl<L<T1, T...>, V, F>
{
    using rest = typename mp_reverse_fold_impl<L<T...>, V, F>::type;
    using type = F<T1, rest>;
};

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class... T, class V, template<class...> class F> struct mp_reverse_fold_impl<L<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T...>, V, F>
{
    using rest = typename mp_reverse_fold_impl<L<T...>, V, F>::type;
    using type = F<T1, F<T2, F<T3, F<T4, F<T5, F<T6, F<T7, F<T8, F<T9, F<T10, rest> > > > > > > > > >;
};

} // namespace detail

template<class L, class V, template<class...> class F> using mp_reverse_fold = typename detail::mp_reverse_fold_impl<L, V, F>::type;
template<class L, class V, class Q> using mp_reverse_fold_q = mp_reverse_fold<L, V, Q::template fn>;

// mp_unique<L>
namespace detail
{

template<class L> struct mp_unique_impl;

template<template<class...> class L, class... T> struct mp_unique_impl<L<T...>>
{
    using type = mp_set_push_back<L<>, T...>;
};

} // namespace detail

template<class L> using mp_unique = typename detail::mp_unique_impl<L>::type;

// mp_all_of<L, P>
template<class L, template<class...> class P> using mp_all_of = mp_bool< mp_count_if<L, P>::value == mp_size<L>::value >;
template<class L, class Q> using mp_all_of_q = mp_all_of<L, Q::template fn>;

// mp_none_of<L, P>
template<class L, template<class...> class P> using mp_none_of = mp_bool< mp_count_if<L, P>::value == 0 >;
template<class L, class Q> using mp_none_of_q = mp_none_of<L, Q::template fn>;

// mp_any_of<L, P>
template<class L, template<class...> class P> using mp_any_of = mp_bool< mp_count_if<L, P>::value != 0 >;
template<class L, class Q> using mp_any_of_q = mp_any_of<L, Q::template fn>;

// mp_replace_at_c<L, I, W>
namespace detail
{

template<class L, class I, class W> struct mp_replace_at_impl
{
    static_assert( I::value >= 0, "mp_replace_at<L, I, W>: I must not be negative" );

    template<class T1, class T2> using _p = std::is_same<T2, mp_size_t<I::value>>;
    template<class T1, class T2> using _f = W;

    using type = mp_transform_if<_p, _f, L, mp_iota<mp_size<L> > >;
};

} // namespace detail

template<class L, class I, class W> using mp_replace_at = typename detail::mp_replace_at_impl<L, I, W>::type;
template<class L, std::size_t I, class W> using mp_replace_at_c = typename detail::mp_replace_at_impl<L, mp_size_t<I>, W>::type;

//mp_for_each<L>(f)
namespace detail
{

template<class... T, class F> BOOST_CONSTEXPR F mp_for_each_impl( mp_list<T...>, F && f )
{
    using A = int[sizeof...(T)];
    return (void)A{ ((void)f(T()), 0)... }, std::forward<F>(f);
}

template<class F> BOOST_CONSTEXPR F mp_for_each_impl( mp_list<>, F && f )
{
    return std::forward<F>(f);
}

} // namespace detail

template<class L, class F> BOOST_CONSTEXPR F mp_for_each( F && f )
{
    return detail::mp_for_each_impl( mp_rename<L, mp_list>(), std::forward<F>(f) );
}

// mp_insert<L, I, T...>
template<class L, class I, class... T> using mp_insert = mp_append<mp_take<L, I>, mp_push_front<mp_drop<L, I>, T...>>;

// mp_insert_c<L, I, T...>
template<class L, std::size_t I, class... T> using mp_insert_c = mp_append<mp_take_c<L, I>, mp_push_front<mp_drop_c<L, I>, T...>>;

// mp_erase<L, I, J>
template<class L, class I, class J> using mp_erase = mp_append<mp_take<L, I>, mp_drop<L, J>>;

// mp_erase_c<L, I, J>
template<class L, std::size_t I, std::size_t J> using mp_erase_c = mp_append<mp_take_c<L, I>, mp_drop_c<L, J>>;

// mp_min_element<L, P>
// mp_max_element<L, P>
//   in detail/mp_min_element.hpp

} // namespace mp11
} // namespace boost

#endif // #ifndef BOOST_MP11_ALGORITHM_HPP_INCLUDED
