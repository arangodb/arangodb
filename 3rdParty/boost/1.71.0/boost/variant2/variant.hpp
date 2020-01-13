#ifndef BOOST_VARIANT2_VARIANT_HPP_INCLUDED
#define BOOST_VARIANT2_VARIANT_HPP_INCLUDED

// Copyright 2017-2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER) && _MSC_VER < 1910
# pragma warning( push )
# pragma warning( disable: 4521 4522 ) // multiple copy operators
#endif

#ifndef BOOST_MP11_HPP_INCLUDED
#include <boost/mp11.hpp>
#endif
#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>
#include <cstddef>
#include <type_traits>
#include <exception>
#include <cassert>
#include <initializer_list>
#include <utility>

//

namespace boost
{

#ifdef BOOST_NO_EXCEPTIONS

BOOST_NORETURN void throw_exception( std::exception const & e ); // user defined

#endif

namespace variant2
{

// bad_variant_access

class bad_variant_access: public std::exception
{
public:

    bad_variant_access() noexcept
    {
    }

    char const * what() const noexcept
    {
        return "bad_variant_access";
    }
};

namespace detail
{

BOOST_NORETURN inline void throw_bad_variant_access()
{
#ifdef BOOST_NO_EXCEPTIONS

    boost::throw_exception( bad_variant_access() );

#else

    throw bad_variant_access();

#endif
}

} // namespace detail

// monostate

struct monostate
{
};

constexpr bool operator<(monostate, monostate) noexcept { return false; }
constexpr bool operator>(monostate, monostate) noexcept { return false; }
constexpr bool operator<=(monostate, monostate) noexcept { return true; }
constexpr bool operator>=(monostate, monostate) noexcept { return true; }
constexpr bool operator==(monostate, monostate) noexcept { return true; }
constexpr bool operator!=(monostate, monostate) noexcept { return false; }

// variant forward declaration

template<class... T> class variant;

// variant_size

template<class T> struct variant_size
{
};

template<class T> struct variant_size<T const>: variant_size<T>
{
};

template<class T> struct variant_size<T volatile>: variant_size<T>
{
};

template<class T> struct variant_size<T const volatile>: variant_size<T>
{
};

template<class T> struct variant_size<T&>: variant_size<T>
{
};

template<class T> struct variant_size<T&&>: variant_size<T>
{
};

#if !defined(BOOST_NO_CXX14_VARIABLE_TEMPLATES)

template <class T> /*inline*/ constexpr std::size_t variant_size_v = variant_size<T>::value;

#endif

template <class... T> struct variant_size<variant<T...>>: mp11::mp_size<variant<T...>>
{
};

// variant_alternative

template<std::size_t I, class T> struct variant_alternative;

template<std::size_t I, class T> using variant_alternative_t = typename variant_alternative<I, T>::type;

#if BOOST_WORKAROUND(BOOST_GCC, < 40900)

namespace detail
{

template<std::size_t I, class T, bool E> struct variant_alternative_impl
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...>, true>
{
    using type = mp11::mp_at_c<variant<T...>, I>;
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> const, true>: std::add_const< mp11::mp_at_c<variant<T...>, I> >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> volatile, true>: std::add_volatile< mp11::mp_at_c<variant<T...>, I> >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> const volatile, true>: std::add_cv< mp11::mp_at_c<variant<T...>, I> >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...>&, true>: std::add_lvalue_reference< mp11::mp_at_c<variant<T...>, I> >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> const&, true>: std::add_lvalue_reference< mp11::mp_at_c<variant<T...>, I> const >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> volatile&, true>: std::add_lvalue_reference< mp11::mp_at_c<variant<T...>, I> volatile >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> const volatile&, true>: std::add_lvalue_reference< mp11::mp_at_c<variant<T...>, I> const volatile >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...>&&, true>: std::add_rvalue_reference< mp11::mp_at_c<variant<T...>, I> >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> const&&, true>: std::add_rvalue_reference< mp11::mp_at_c<variant<T...>, I> const >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> volatile&&, true>: std::add_rvalue_reference< mp11::mp_at_c<variant<T...>, I> volatile >
{
};

template<std::size_t I, class... T> struct variant_alternative_impl<I, variant<T...> const volatile&&, true>: std::add_rvalue_reference< mp11::mp_at_c<variant<T...>, I> const volatile >
{
};

} // namespace detail

template<std::size_t I, class T> struct variant_alternative
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...>>: public detail::variant_alternative_impl<I, variant<T...>, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> const>: public detail::variant_alternative_impl<I, variant<T...> const, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> volatile>: public detail::variant_alternative_impl<I, variant<T...> volatile, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> const volatile>: public detail::variant_alternative_impl<I, variant<T...> const volatile, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...>&>: public detail::variant_alternative_impl<I, variant<T...>&, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> const&>: public detail::variant_alternative_impl<I, variant<T...> const&, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> volatile&>: public detail::variant_alternative_impl<I, variant<T...> volatile&, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> const volatile&>: public detail::variant_alternative_impl<I, variant<T...> const volatile&, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...>&&>: public detail::variant_alternative_impl<I, variant<T...>&&, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> const&&>: public detail::variant_alternative_impl<I, variant<T...> const&&, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> volatile&&>: public detail::variant_alternative_impl<I, variant<T...> volatile&&, (I < sizeof...(T))>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...> const volatile&&>: public detail::variant_alternative_impl<I, variant<T...> const volatile&&, (I < sizeof...(T))>
{
};

#else

namespace detail
{

#if defined( BOOST_MP11_VERSION ) && BOOST_MP11_VERSION >= 107000

template<class I, class T, class Q> using var_alt_impl = mp11::mp_invoke_q<Q, variant_alternative_t<I::value, T>>;

#else

template<class I, class T, class Q> using var_alt_impl = mp11::mp_invoke<Q, variant_alternative_t<I::value, T>>;

#endif

} // namespace detail

template<std::size_t I, class T> struct variant_alternative
{
};

template<std::size_t I, class T> struct variant_alternative<I, T const>: mp11::mp_defer<detail::var_alt_impl, mp11::mp_size_t<I>, T, mp11::mp_quote_trait<std::add_const>>
{
};

template<std::size_t I, class T> struct variant_alternative<I, T volatile>: mp11::mp_defer<detail::var_alt_impl, mp11::mp_size_t<I>, T, mp11::mp_quote_trait<std::add_volatile>>
{
};

template<std::size_t I, class T> struct variant_alternative<I, T const volatile>: mp11::mp_defer<detail::var_alt_impl, mp11::mp_size_t<I>, T, mp11::mp_quote_trait<std::add_cv>>
{
};

template<std::size_t I, class T> struct variant_alternative<I, T&>: mp11::mp_defer<detail::var_alt_impl, mp11::mp_size_t<I>, T, mp11::mp_quote_trait<std::add_lvalue_reference>>
{
};

template<std::size_t I, class T> struct variant_alternative<I, T&&>: mp11::mp_defer<detail::var_alt_impl, mp11::mp_size_t<I>, T, mp11::mp_quote_trait<std::add_rvalue_reference>>
{
};

template<std::size_t I, class... T> struct variant_alternative<I, variant<T...>>: mp11::mp_defer<mp11::mp_at, variant<T...>, mp11::mp_size_t<I>>
{
};

#endif

// variant_npos

constexpr std::size_t variant_npos = ~static_cast<std::size_t>( 0 );

// holds_alternative

template<class U, class... T> constexpr bool holds_alternative( variant<T...> const& v ) noexcept
{
    static_assert( mp11::mp_count<variant<T...>, U>::value == 1, "The type must occur exactly once in the list of variant alternatives" );
    return v.index() == mp11::mp_find<variant<T...>, U>::value;
}

// get (index)

template<std::size_t I, class... T> constexpr variant_alternative_t<I, variant<T...>>& get(variant<T...>& v)
{
    static_assert( I < sizeof...(T), "Index out of bounds" );
    return ( v.index() != I? detail::throw_bad_variant_access(): (void)0 ), v._get_impl( mp11::mp_size_t<I>() );
}

template<std::size_t I, class... T> constexpr variant_alternative_t<I, variant<T...>>&& get(variant<T...>&& v)
{
    static_assert( I < sizeof...(T), "Index out of bounds" );

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1930)

    return ( v.index() != I? detail::throw_bad_variant_access(): (void)0 ), std::move( v._get_impl( mp11::mp_size_t<I>() ) );

#else

    if( v.index() != I ) detail::throw_bad_variant_access();
    return std::move( v._get_impl( mp11::mp_size_t<I>() ) );

#endif
}

template<std::size_t I, class... T> constexpr variant_alternative_t<I, variant<T...>> const& get(variant<T...> const& v)
{
    static_assert( I < sizeof...(T), "Index out of bounds" );
    return ( v.index() != I? detail::throw_bad_variant_access(): (void)0 ), v._get_impl( mp11::mp_size_t<I>() );
}

template<std::size_t I, class... T> constexpr variant_alternative_t<I, variant<T...>> const&& get(variant<T...> const&& v)
{
    static_assert( I < sizeof...(T), "Index out of bounds" );

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1930)

    return ( v.index() != I? detail::throw_bad_variant_access(): (void)0 ), std::move( v._get_impl( mp11::mp_size_t<I>() ) );

#else

    if( v.index() != I ) detail::throw_bad_variant_access();
    return std::move( v._get_impl( mp11::mp_size_t<I>() ) );

#endif
}

// detail::unsafe_get (for visit)

namespace detail
{

template<std::size_t I, class... T> constexpr variant_alternative_t<I, variant<T...>>& unsafe_get(variant<T...>& v)
{
    static_assert( I < sizeof...(T), "Index out of bounds" );
    return v._get_impl( mp11::mp_size_t<I>() );
}

template<std::size_t I, class... T> constexpr variant_alternative_t<I, variant<T...>>&& unsafe_get(variant<T...>&& v)
{
    static_assert( I < sizeof...(T), "Index out of bounds" );
    return std::move( v._get_impl( mp11::mp_size_t<I>() ) );
}

template<std::size_t I, class... T> constexpr variant_alternative_t<I, variant<T...>> const& unsafe_get(variant<T...> const& v)
{
    static_assert( I < sizeof...(T), "Index out of bounds" );
    return v._get_impl( mp11::mp_size_t<I>() );
}

template<std::size_t I, class... T> constexpr variant_alternative_t<I, variant<T...>> const&& unsafe_get(variant<T...> const&& v)
{
    static_assert( I < sizeof...(T), "Index out of bounds" );
    return std::move( v._get_impl( mp11::mp_size_t<I>() ) );
}

} // namespace detail

// get (type)

template<class U, class... T> constexpr U& get(variant<T...>& v)
{
    static_assert( mp11::mp_count<variant<T...>, U>::value == 1, "The type must occur exactly once in the list of variant alternatives" );

    using I = mp11::mp_find<variant<T...>, U>;

    return ( v.index() != I::value? detail::throw_bad_variant_access(): (void)0 ), v._get_impl( I() );
}

template<class U, class... T> constexpr U&& get(variant<T...>&& v)
{
    static_assert( mp11::mp_count<variant<T...>, U>::value == 1, "The type must occur exactly once in the list of variant alternatives" );

    using I = mp11::mp_find<variant<T...>, U>;

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1930)

    return ( v.index() != I::value? detail::throw_bad_variant_access(): (void)0 ), std::move( v._get_impl( I() ) );

#else

    if( v.index() != I::value ) detail::throw_bad_variant_access();
    return std::move( v._get_impl( I() ) );

#endif
}

template<class U, class... T> constexpr U const& get(variant<T...> const& v)
{
    static_assert( mp11::mp_count<variant<T...>, U>::value == 1, "The type must occur exactly once in the list of variant alternatives" );

    using I = mp11::mp_find<variant<T...>, U>;

    return ( v.index() != I::value? detail::throw_bad_variant_access(): (void)0 ), v._get_impl( I() );
}

template<class U, class... T> constexpr U const&& get(variant<T...> const&& v)
{
    static_assert( mp11::mp_count<variant<T...>, U>::value == 1, "The type must occur exactly once in the list of variant alternatives" );

    using I = mp11::mp_find<variant<T...>, U>;

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1930)

    return ( v.index() != I::value? detail::throw_bad_variant_access(): (void)0 ), std::move( v._get_impl( I() ) );

#else

    if( v.index() != I::value ) detail::throw_bad_variant_access();
    return std::move( v._get_impl( I() ) );

#endif
}

// get_if

template<std::size_t I, class... T> constexpr typename std::add_pointer<variant_alternative_t<I, variant<T...>>>::type get_if(variant<T...>* v) noexcept
{
    static_assert( I < sizeof...(T), "Index out of bounds" );
    return v && v->index() == I? &v->_get_impl( mp11::mp_size_t<I>() ): 0;
}

template<std::size_t I, class... T> constexpr typename std::add_pointer<const variant_alternative_t<I, variant<T...>>>::type get_if(variant<T...> const * v) noexcept
{
    static_assert( I < sizeof...(T), "Index out of bounds" );
    return v && v->index() == I? &v->_get_impl( mp11::mp_size_t<I>() ): 0;
}

template<class U, class... T> constexpr typename std::add_pointer<U>::type get_if(variant<T...>* v) noexcept
{
    static_assert( mp11::mp_count<variant<T...>, U>::value == 1, "The type must occur exactly once in the list of variant alternatives" );

    using I = mp11::mp_find<variant<T...>, U>;

    return v && v->index() == I::value? &v->_get_impl( I() ): 0;
}

template<class U, class... T> constexpr typename std::add_pointer<U const>::type get_if(variant<T...> const * v) noexcept
{
    static_assert( mp11::mp_count<variant<T...>, U>::value == 1, "The type must occur exactly once in the list of variant alternatives" );

    using I = mp11::mp_find<variant<T...>, U>;

    return v && v->index() == I::value? &v->_get_impl( I() ): 0;
}

//

namespace detail
{

// trivially_*

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000

template<class T> struct is_trivially_copy_constructible: mp11::mp_bool<std::is_copy_constructible<T>::value && std::has_trivial_copy_constructor<T>::value>
{
};

template<class T> struct is_trivially_copy_assignable: mp11::mp_bool<std::is_copy_assignable<T>::value && std::has_trivial_copy_assign<T>::value>
{
};

template<class T> struct is_trivially_move_constructible: mp11::mp_bool<std::is_move_constructible<T>::value && std::is_trivial<T>::value>
{
};

template<class T> struct is_trivially_move_assignable: mp11::mp_bool<std::is_move_assignable<T>::value && std::is_trivial<T>::value>
{
};

#else

using std::is_trivially_copy_constructible;
using std::is_trivially_copy_assignable;
using std::is_trivially_move_constructible;
using std::is_trivially_move_assignable;

#endif

// variant_storage

template<class D, class... T> union variant_storage_impl;

template<class... T> using variant_storage = variant_storage_impl<mp11::mp_all<std::is_trivially_destructible<T>...>, T...>;

template<class D> union variant_storage_impl<D>
{
};

// not all trivially destructible
template<class T1, class... T> union variant_storage_impl<mp11::mp_false, T1, T...>
{
    T1 first_;
    variant_storage<T...> rest_;

    template<class... A> constexpr explicit variant_storage_impl( mp11::mp_size_t<0>, A&&... a ): first_( std::forward<A>(a)... )
    {
    }

    template<std::size_t I, class... A> constexpr explicit variant_storage_impl( mp11::mp_size_t<I>, A&&... a ): rest_( mp11::mp_size_t<I-1>(), std::forward<A>(a)... )
    {
    }

    ~variant_storage_impl()
    {
    }

    template<class... A> void emplace( mp11::mp_size_t<0>, A&&... a )
    {
        ::new( &first_ ) T1( std::forward<A>(a)... );
    }

    template<std::size_t I, class... A> void emplace( mp11::mp_size_t<I>, A&&... a )
    {
        rest_.emplace( mp11::mp_size_t<I-1>(), std::forward<A>(a)... );
    }

    BOOST_CXX14_CONSTEXPR T1& get( mp11::mp_size_t<0> ) noexcept { return first_; }
    constexpr T1 const& get( mp11::mp_size_t<0> ) const noexcept { return first_; }

    template<std::size_t I> BOOST_CXX14_CONSTEXPR mp11::mp_at_c<mp11::mp_list<T...>, I-1>& get( mp11::mp_size_t<I> ) noexcept { return rest_.get( mp11::mp_size_t<I-1>() ); }
    template<std::size_t I> constexpr mp11::mp_at_c<mp11::mp_list<T...>, I-1> const& get( mp11::mp_size_t<I> ) const noexcept { return rest_.get( mp11::mp_size_t<I-1>() ); }
};

// all trivially destructible
template<class T1, class... T> union variant_storage_impl<mp11::mp_true, T1, T...>
{
    T1 first_;
    variant_storage<T...> rest_;

    template<class... A> constexpr explicit variant_storage_impl( mp11::mp_size_t<0>, A&&... a ): first_( std::forward<A>(a)... )
    {
    }

    template<std::size_t I, class... A> constexpr explicit variant_storage_impl( mp11::mp_size_t<I>, A&&... a ): rest_( mp11::mp_size_t<I-1>(), std::forward<A>(a)... )
    {
    }

    template<class... A> void emplace_impl( mp11::mp_false, mp11::mp_size_t<0>, A&&... a )
    {
        ::new( &first_ ) T1( std::forward<A>(a)... );
    }

    template<std::size_t I, class... A> BOOST_CXX14_CONSTEXPR void emplace_impl( mp11::mp_false, mp11::mp_size_t<I>, A&&... a )
    {
        rest_.emplace( mp11::mp_size_t<I-1>(), std::forward<A>(a)... );
    }

    template<std::size_t I, class... A> BOOST_CXX14_CONSTEXPR void emplace_impl( mp11::mp_true, mp11::mp_size_t<I>, A&&... a )
    {
        *this = variant_storage_impl( mp11::mp_size_t<I>(), std::forward<A>(a)... );
    }

    template<std::size_t I, class... A> BOOST_CXX14_CONSTEXPR void emplace( mp11::mp_size_t<I>, A&&... a )
    {
        this->emplace_impl( mp11::mp_all<detail::is_trivially_move_assignable<T1>, detail::is_trivially_move_assignable<T>...>(), mp11::mp_size_t<I>(), std::forward<A>(a)... );
    }

    BOOST_CXX14_CONSTEXPR T1& get( mp11::mp_size_t<0> ) noexcept { return first_; }
    constexpr T1 const& get( mp11::mp_size_t<0> ) const noexcept { return first_; }

    template<std::size_t I> BOOST_CXX14_CONSTEXPR mp11::mp_at_c<mp11::mp_list<T...>, I-1>& get( mp11::mp_size_t<I> ) noexcept { return rest_.get( mp11::mp_size_t<I-1>() ); }
    template<std::size_t I> constexpr mp11::mp_at_c<mp11::mp_list<T...>, I-1> const& get( mp11::mp_size_t<I> ) const noexcept { return rest_.get( mp11::mp_size_t<I-1>() ); }
};

// resolve_overload_*

template<class... T> struct overload;

template<> struct overload<>
{
    void operator()() const;
};

template<class T1, class... T> struct overload<T1, T...>: overload<T...>
{
    using overload<T...>::operator();
    mp11::mp_identity<T1> operator()(T1) const;
};

#if BOOST_WORKAROUND( BOOST_MSVC, < 1930 )

template<class U, class... T> using resolve_overload_type_ = decltype( overload<T...>()(std::declval<U>()) );

template<class U, class... T> struct resolve_overload_type_impl: mp11::mp_defer< resolve_overload_type_, U, T... >
{
};

template<class U, class... T> using resolve_overload_type = typename resolve_overload_type_impl<U, T...>::type::type;

#else

template<class U, class... T> using resolve_overload_type = typename decltype( overload<T...>()(std::declval<U>()) )::type;

#endif

template<class U, class... T> using resolve_overload_index = mp11::mp_find<mp11::mp_list<T...>, resolve_overload_type<U, T...>>;

// variant_base

template<bool is_trivially_destructible, bool is_single_buffered, class... T> struct variant_base_impl;
template<class... T> using variant_base = variant_base_impl<mp11::mp_all<std::is_trivially_destructible<T>...>::value, mp11::mp_all<std::is_nothrow_move_constructible<T>...>::value, T...>;

struct none {};

// trivially destructible, single buffered
template<class... T> struct variant_base_impl<true, true, T...>
{
    int ix_;
    variant_storage<none, T...> st1_;

    constexpr variant_base_impl(): ix_( 0 ), st1_( mp11::mp_size_t<0>() )
    {
    }

    template<class I, class... A> constexpr explicit variant_base_impl( I, A&&... a ): ix_( I::value + 1 ), st1_( mp11::mp_size_t<I::value + 1>(), std::forward<A>(a)... )
    {
    }

    // requires: ix_ == 0
    template<class I, class... A> void _replace( I, A&&... a )
    {
        ::new( &st1_ ) variant_storage<none, T...>( mp11::mp_size_t<I::value + 1>(), std::forward<A>(a)... );
        ix_ = I::value + 1;
    }

    constexpr std::size_t index() const noexcept
    {
        return ix_ - 1;
    }

    template<std::size_t I> BOOST_CXX14_CONSTEXPR mp11::mp_at_c<variant<T...>, I>& _get_impl( mp11::mp_size_t<I> ) noexcept
    {
        size_t const J = I+1;

        assert( ix_ == J );

        return st1_.get( mp11::mp_size_t<J>() );
    }

    template<std::size_t I> constexpr mp11::mp_at_c<variant<T...>, I> const& _get_impl( mp11::mp_size_t<I> ) const noexcept
    {
        // size_t const J = I+1;
        // assert( ix_ == I+1 );

        return st1_.get( mp11::mp_size_t<I+1>() );
    }

    template<std::size_t J, class U, class... A> BOOST_CXX14_CONSTEXPR void emplace_impl( mp11::mp_true, A&&... a )
    {
        static_assert( std::is_nothrow_constructible<U, A&&...>::value, "Logic error: U must be nothrow constructible from A&&..." );

        st1_.emplace( mp11::mp_size_t<J>(), std::forward<A>(a)... );
        ix_ = J;
    }

    template<std::size_t J, class U, class... A> BOOST_CXX14_CONSTEXPR void emplace_impl( mp11::mp_false, A&&... a )
    {
        static_assert( std::is_nothrow_move_constructible<U>::value, "Logic error: U must be nothrow move constructible" );

        U tmp( std::forward<A>(a)... );

        st1_.emplace( mp11::mp_size_t<J>(), std::move(tmp) );
        ix_ = J;
    }

    template<std::size_t I, class... A> BOOST_CXX14_CONSTEXPR void emplace( A&&... a )
    {
        std::size_t const J = I+1;
        using U = mp11::mp_at_c<variant<T...>, I>;

        this->emplace_impl<J, U>( std::is_nothrow_constructible<U, A&&...>(), std::forward<A>(a)... );
    }
};

// trivially destructible, double buffered
template<class... T> struct variant_base_impl<true, false, T...>
{
    int ix_;
    variant_storage<none, T...> st1_;
    variant_storage<none, T...> st2_;

    constexpr variant_base_impl(): ix_( 0 ), st1_( mp11::mp_size_t<0>() ), st2_( mp11::mp_size_t<0>() )
    {
    }

    template<class I, class... A> constexpr explicit variant_base_impl( I, A&&... a ): ix_( I::value + 1 ), st1_( mp11::mp_size_t<I::value + 1>(), std::forward<A>(a)... ), st2_( mp11::mp_size_t<0>() )
    {
    }

    // requires: ix_ == 0
    template<class I, class... A> void _replace( I, A&&... a )
    {
        ::new( &st1_ ) variant_storage<none, T...>( mp11::mp_size_t<I::value + 1>(), std::forward<A>(a)... );
        ix_ = I::value + 1;
    }

    constexpr std::size_t index() const noexcept
    {
        return ix_ >= 0? ix_ - 1: -ix_ - 1;
    }

    template<std::size_t I> BOOST_CXX14_CONSTEXPR mp11::mp_at_c<variant<T...>, I>& _get_impl( mp11::mp_size_t<I> ) noexcept
    {
        size_t const J = I+1;

        assert( ix_ == J || -ix_ == J );

        constexpr mp11::mp_size_t<J> j{};
        return ix_ >= 0? st1_.get( j ): st2_.get( j );
    }

    template<std::size_t I> constexpr mp11::mp_at_c<variant<T...>, I> const& _get_impl( mp11::mp_size_t<I> ) const noexcept
    {
        // size_t const J = I+1;
        // assert( ix_ == J || -ix_ == J );
        // constexpr mp_size_t<J> j{};

        return ix_ >= 0? st1_.get( mp11::mp_size_t<I+1>() ): st2_.get( mp11::mp_size_t<I+1>() );
    }

    template<std::size_t I, class... A> BOOST_CXX14_CONSTEXPR void emplace( A&&... a )
    {
        size_t const J = I+1;

        if( ix_ >= 0 )
        {
            st2_.emplace( mp11::mp_size_t<J>(), std::forward<A>(a)... );
            ix_ = -static_cast<int>( J );
        }
        else
        {
            st1_.emplace( mp11::mp_size_t<J>(), std::forward<A>(a)... );
            ix_ = J;
        }
    }
};

// not trivially destructible, single buffered
template<class... T> struct variant_base_impl<false, true, T...>
{
    int ix_;
    variant_storage<none, T...> st1_;

    constexpr variant_base_impl(): ix_( 0 ), st1_( mp11::mp_size_t<0>() )
    {
    }

    template<class I, class... A> constexpr explicit variant_base_impl( I, A&&... a ): ix_( I::value + 1 ), st1_( mp11::mp_size_t<I::value + 1>(), std::forward<A>(a)... )
    {
    }

    // requires: ix_ == 0
    template<class I, class... A> void _replace( I, A&&... a )
    {
        ::new( &st1_ ) variant_storage<none, T...>( mp11::mp_size_t<I::value + 1>(), std::forward<A>(a)... );
        ix_ = I::value + 1;
    }

    //[&]( auto I ){
    //    using U = mp_at_c<mp_list<none, T...>, I>;
    //    st1_.get( I ).~U();
    //}

    struct _destroy_L1
    {
        variant_base_impl * this_;

        template<class I> void operator()( I ) const noexcept
        {
            using U = mp11::mp_at<mp11::mp_list<none, T...>, I>;
            this_->st1_.get( I() ).~U();
        }
    };

    void _destroy() noexcept
    {
        if( ix_ > 0 )
        {
            mp11::mp_with_index<1 + sizeof...(T)>( ix_, _destroy_L1{ this } );
        }
    }

    ~variant_base_impl() noexcept
    {
        _destroy();
    }

    constexpr std::size_t index() const noexcept
    {
        return ix_ - 1;
    }

    template<std::size_t I> BOOST_CXX14_CONSTEXPR mp11::mp_at_c<variant<T...>, I>& _get_impl( mp11::mp_size_t<I> ) noexcept
    {
        size_t const J = I+1;

        assert( ix_ == J );

        return st1_.get( mp11::mp_size_t<J>() );
    }

    template<std::size_t I> constexpr mp11::mp_at_c<variant<T...>, I> const& _get_impl( mp11::mp_size_t<I> ) const noexcept
    {
        // size_t const J = I+1;
        // assert( ix_ == J );

        return st1_.get( mp11::mp_size_t<I+1>() );
    }

    template<std::size_t I, class... A> void emplace( A&&... a )
    {
        size_t const J = I+1;

        using U = mp11::mp_at_c<variant<T...>, I>;

        static_assert( std::is_nothrow_move_constructible<U>::value, "Logic error: U must be nothrow move constructible" );

        U tmp( std::forward<A>(a)... );

        _destroy();

        st1_.emplace( mp11::mp_size_t<J>(), std::move(tmp) );
        ix_ = J;
    }
};

// not trivially destructible, double buffered
template<class... T> struct variant_base_impl<false, false, T...>
{
    int ix_;
    variant_storage<none, T...> st1_;
    variant_storage<none, T...> st2_;

    constexpr variant_base_impl(): ix_( 0 ), st1_( mp11::mp_size_t<0>() ), st2_( mp11::mp_size_t<0>() )
    {
    }

    template<class I, class... A> constexpr explicit variant_base_impl( I, A&&... a ): ix_( I::value + 1 ), st1_( mp11::mp_size_t<I::value + 1>(), std::forward<A>(a)... ), st2_( mp11::mp_size_t<0>() )
    {
    }

    // requires: ix_ == 0
    template<class I, class... A> void _replace( I, A&&... a )
    {
        ::new( &st1_ ) variant_storage<none, T...>( mp11::mp_size_t<I::value + 1>(), std::forward<A>(a)... );
        ix_ = I::value + 1;
    }

    //[&]( auto I ){
    //    using U = mp_at_c<mp_list<none, T...>, I>;
    //    st1_.get( I ).~U();
    //}

    struct _destroy_L1
    {
        variant_base_impl * this_;

        template<class I> void operator()( I ) const noexcept
        {
            using U = mp11::mp_at<mp11::mp_list<none, T...>, I>;
            this_->st1_.get( I() ).~U();
        }
    };

    struct _destroy_L2
    {
        variant_base_impl * this_;

        template<class I> void operator()( I ) const noexcept
        {
            using U = mp11::mp_at<mp11::mp_list<none, T...>, I>;
            this_->st2_.get( I() ).~U();
        }
    };

    void _destroy() noexcept
    {
        if( ix_ > 0 )
        {
            mp11::mp_with_index<1 + sizeof...(T)>( ix_, _destroy_L1{ this } );
        }
        else if( ix_ < 0 )
        {
            mp11::mp_with_index<1 + sizeof...(T)>( -ix_, _destroy_L2{ this } );
        }
    }

    ~variant_base_impl() noexcept
    {
        _destroy();
    }

    constexpr std::size_t index() const noexcept
    {
        return ix_ >= 0? ix_ - 1: -ix_ - 1;
    }

    template<std::size_t I> BOOST_CXX14_CONSTEXPR mp11::mp_at_c<variant<T...>, I>& _get_impl( mp11::mp_size_t<I> ) noexcept
    {
        size_t const J = I+1;

        assert( ix_ == J || -ix_ == J );

        constexpr mp11::mp_size_t<J> j{};
        return ix_ >= 0? st1_.get( j ): st2_.get( j );
    }

    template<std::size_t I> constexpr mp11::mp_at_c<variant<T...>, I> const& _get_impl( mp11::mp_size_t<I> ) const noexcept
    {
        // size_t const J = I+1;
        // assert( ix_ == J || -ix_ == J );
        // constexpr mp_size_t<J> j{};

        return ix_ >= 0? st1_.get( mp11::mp_size_t<I+1>() ): st2_.get( mp11::mp_size_t<I+1>() );
    }

    template<std::size_t I, class... A> void emplace( A&&... a )
    {
        size_t const J = I+1;

        if( ix_ >= 0 )
        {
            st2_.emplace( mp11::mp_size_t<J>(), std::forward<A>(a)... );
            _destroy();

            ix_ = -static_cast<int>( J );
        }
        else
        {
            st1_.emplace( mp11::mp_size_t<J>(), std::forward<A>(a)... );
            _destroy();

            ix_ = J;
        }
    }
};

} // namespace detail

// in_place_type_t

template<class T> struct in_place_type_t
{
};

#if !defined(BOOST_NO_CXX14_VARIABLE_TEMPLATES)

template<class T> constexpr in_place_type_t<T> in_place_type{};

#endif

namespace detail
{

template<class T> struct is_in_place_type: std::false_type {};
template<class T> struct is_in_place_type<in_place_type_t<T>>: std::true_type {};

} // namespace detail

// in_place_index_t

template<std::size_t I> struct in_place_index_t
{
};

#if !defined(BOOST_NO_CXX14_VARIABLE_TEMPLATES)

template<std::size_t I> constexpr in_place_index_t<I> in_place_index{};

#endif

namespace detail
{

template<class T> struct is_in_place_index: std::false_type {};
template<std::size_t I> struct is_in_place_index<in_place_index_t<I>>: std::true_type {};

} // namespace detail

// is_nothrow_swappable

namespace detail
{

namespace det2
{

using std::swap;

template<class T> using is_swappable_impl = decltype(swap(std::declval<T&>(), std::declval<T&>()));

#if BOOST_WORKAROUND( BOOST_MSVC, < 1920 )

template<class T> struct is_nothrow_swappable_impl_
{
    static constexpr bool value = noexcept(swap(std::declval<T&>(), std::declval<T&>()));
};

template<class T> using is_nothrow_swappable_impl = mp11::mp_bool< is_nothrow_swappable_impl_<T>::value >;

#else

template<class T> using is_nothrow_swappable_impl = typename std::enable_if<noexcept(swap(std::declval<T&>(), std::declval<T&>()))>::type;

#endif

} // namespace det2

template<class T> struct is_swappable: mp11::mp_valid<det2::is_swappable_impl, T>
{
};

#if BOOST_WORKAROUND( BOOST_MSVC, < 1920 )

template<class T> struct is_nothrow_swappable: mp11::mp_eval_if<mp11::mp_not<is_swappable<T>>, mp11::mp_false, det2::is_nothrow_swappable_impl, T>
{
};

#else

template<class T> struct is_nothrow_swappable: mp11::mp_valid<det2::is_nothrow_swappable_impl, T>
{
};

#endif

} // namespace detail

// variant

template<class... T> class variant: private detail::variant_base<T...>
{
private:

    using variant_base = detail::variant_base<T...>;

private:

    variant( variant const volatile& r ) = delete;
    variant& operator=( variant const volatile& r ) = delete;

public:

    // constructors

    template<class E1 = void, class E2 = mp11::mp_if<std::is_default_constructible< mp11::mp_first<variant<T...>> >, E1>>
    constexpr variant()
        noexcept( std::is_nothrow_default_constructible< mp11::mp_first<variant<T...>> >::value )
        : variant_base( mp11::mp_size_t<0>() )
    {
    }

    template<class E1 = void,
        class E2 = mp11::mp_if<mp11::mp_all<detail::is_trivially_copy_constructible<T>...>, E1>
    >
    constexpr variant( variant const& r ) noexcept
        : variant_base( static_cast<variant_base const&>(r) )
    {
    }

private:

    struct L1
    {
        variant_base * this_;
        variant const & r;

        template<class I> void operator()( I i ) const
        {
            this_->_replace( i, r._get_impl( i ) );
        }
    };

public:

    template<class E1 = void,
        class E2 = mp11::mp_if<mp11::mp_not<mp11::mp_all<detail::is_trivially_copy_constructible<T>...>>, E1>,
        class E3 = mp11::mp_if<mp11::mp_all<std::is_copy_constructible<T>...>, E1>
    >
    variant( variant const& r )
        noexcept( mp11::mp_all<std::is_nothrow_copy_constructible<T>...>::value )
    {
        mp11::mp_with_index<sizeof...(T)>( r.index(), L1{ this, r } );
    }

    template<class E1 = void,
        class E2 = mp11::mp_if<mp11::mp_all<detail::is_trivially_move_constructible<T>...>, E1>
    >
    constexpr variant( variant && r ) noexcept
        : variant_base( static_cast<variant_base&&>(r) )
    {
    }

private:

    struct L2
    {
        variant_base * this_;
        variant & r;

        template<class I> void operator()( I i ) const
        {
            this_->_replace( i, std::move( r._get_impl( i ) ) );
        }
    };

public:

    template<class E1 = void,
        class E2 = mp11::mp_if<mp11::mp_not<mp11::mp_all<detail::is_trivially_move_constructible<T>...>>, E1>,
        class E3 = mp11::mp_if<mp11::mp_all<std::is_move_constructible<T>...>, E1>
    >
    variant( variant && r )
        noexcept( mp11::mp_all<std::is_nothrow_move_constructible<T>...>::value )
    {
        mp11::mp_with_index<sizeof...(T)>( r.index(), L2{ this, r } );
    }

    template<class U,
        class Ud = typename std::decay<U>::type,
        class E1 = typename std::enable_if< !std::is_same<Ud, variant>::value && !detail::is_in_place_index<Ud>::value && !detail::is_in_place_type<Ud>::value >::type,
        class V = detail::resolve_overload_type<U&&, T...>,
        class E2 = typename std::enable_if<std::is_constructible<V, U&&>::value>::type
        >
    constexpr variant( U&& u )
        noexcept( std::is_nothrow_constructible<V, U&&>::value )
        : variant_base( detail::resolve_overload_index<U&&, T...>(), std::forward<U>(u) )
    {
    }

    template<class U, class... A, class I = mp11::mp_find<variant<T...>, U>, class E = typename std::enable_if<std::is_constructible<U, A&&...>::value>::type>
    constexpr explicit variant( in_place_type_t<U>, A&&... a ): variant_base( I(), std::forward<A>(a)... )
    {
    }

    template<class U, class V, class... A, class I = mp11::mp_find<variant<T...>, U>, class E = typename std::enable_if<std::is_constructible<U, std::initializer_list<V>&, A&&...>::value>::type>
    constexpr explicit variant( in_place_type_t<U>, std::initializer_list<V> il, A&&... a ): variant_base( I(), il, std::forward<A>(a)... )
    {
    }

    template<std::size_t I, class... A, class E = typename std::enable_if<std::is_constructible<mp11::mp_at_c<variant<T...>, I>, A&&...>::value>::type>
    constexpr explicit variant( in_place_index_t<I>, A&&... a ): variant_base( mp11::mp_size_t<I>(), std::forward<A>(a)... )
    {
    }

    template<std::size_t I, class V, class... A, class E = typename std::enable_if<std::is_constructible<mp11::mp_at_c<variant<T...>, I>, std::initializer_list<V>&, A&&...>::value>::type>
    constexpr explicit variant( in_place_index_t<I>, std::initializer_list<V> il, A&&... a ): variant_base( mp11::mp_size_t<I>(), il, std::forward<A>(a)... )
    {
    }

    // assignment
    template<class E1 = void,
        class E2 = mp11::mp_if<mp11::mp_all<std::is_trivially_destructible<T>..., detail::is_trivially_copy_constructible<T>..., detail::is_trivially_copy_assignable<T>...>, E1>
    >
    BOOST_CXX14_CONSTEXPR variant& operator=( variant const & r ) noexcept
    {
        static_cast<variant_base&>( *this ) = static_cast<variant_base const&>( r );
        return *this;
    }

private:

    struct L3
    {
        variant * this_;
        variant const & r;

        template<class I> void operator()( I i ) const
        {
            this_->variant_base::template emplace<I::value>( r._get_impl( i ) );
        }
    };

public:

    template<class E1 = void,
        class E2 = mp11::mp_if<mp11::mp_not<mp11::mp_all<std::is_trivially_destructible<T>..., detail::is_trivially_copy_constructible<T>..., detail::is_trivially_copy_assignable<T>...>>, E1>,
        class E3 = mp11::mp_if<mp11::mp_all<std::is_copy_constructible<T>..., std::is_copy_assignable<T>...>, E1>
    >
    BOOST_CXX14_CONSTEXPR variant& operator=( variant const & r )
        noexcept( mp11::mp_all<std::is_nothrow_copy_constructible<T>...>::value )
    {
        mp11::mp_with_index<sizeof...(T)>( r.index(), L3{ this, r } );
        return *this;
    }

    template<class E1 = void,
        class E2 = mp11::mp_if<mp11::mp_all<std::is_trivially_destructible<T>..., detail::is_trivially_move_constructible<T>..., detail::is_trivially_move_assignable<T>...>, E1>
    >
    BOOST_CXX14_CONSTEXPR variant& operator=( variant && r ) noexcept
    {
        static_cast<variant_base&>( *this ) = static_cast<variant_base&&>( r );
        return *this;
    }

private:

    struct L4
    {
        variant * this_;
        variant & r;

        template<class I> void operator()( I i ) const
        {
            this_->variant_base::template emplace<I::value>( std::move( r._get_impl( i ) ) );
        }
    };

public:

    template<class E1 = void,
        class E2 = mp11::mp_if<mp11::mp_not<mp11::mp_all<std::is_trivially_destructible<T>..., detail::is_trivially_move_constructible<T>..., detail::is_trivially_move_assignable<T>...>>, E1>,
        class E3 = mp11::mp_if<mp11::mp_all<std::is_move_constructible<T>..., std::is_move_assignable<T>...>, E1>
    >
    variant& operator=( variant && r )
        noexcept( mp11::mp_all<std::is_nothrow_move_constructible<T>...>::value )
    {
        mp11::mp_with_index<sizeof...(T)>( r.index(), L4{ this, r } );
        return *this;
    }

    template<class U,
        class E1 = typename std::enable_if<!std::is_same<typename std::decay<U>::type, variant>::value>::type,
        class V = detail::resolve_overload_type<U, T...>,
        class E2 = typename std::enable_if<std::is_assignable<V&, U&&>::value && std::is_constructible<V, U&&>::value>::type
    >
    BOOST_CXX14_CONSTEXPR variant& operator=( U&& u )
        noexcept( std::is_nothrow_constructible<V, U&&>::value )
    {
        std::size_t const I = detail::resolve_overload_index<U, T...>::value;
        this->template emplace<I>( std::forward<U>(u) );
        return *this;
    }

    // modifiers

    template<class U, class... A,
        class E = typename std::enable_if< mp11::mp_count<variant<T...>, U>::value == 1 && std::is_constructible<U, A&&...>::value >::type>
    BOOST_CXX14_CONSTEXPR U& emplace( A&&... a )
    {
        using I = mp11::mp_find<variant<T...>, U>;
        variant_base::template emplace<I::value>( std::forward<A>(a)... );
        return _get_impl( I() );
    }

    template<class U, class V, class... A,
        class E = typename std::enable_if< mp11::mp_count<variant<T...>, U>::value == 1 && std::is_constructible<U, std::initializer_list<V>&, A&&...>::value >::type>
    BOOST_CXX14_CONSTEXPR U& emplace( std::initializer_list<V> il, A&&... a )
    {
        using I = mp11::mp_find<variant<T...>, U>;
        variant_base::template emplace<I::value>( il, std::forward<A>(a)... );
        return _get_impl( I() );
    }

    template<std::size_t I, class... A, class E = typename std::enable_if<std::is_constructible<mp11::mp_at_c<variant<T...>, I>, A&&...>::value>::type>
    BOOST_CXX14_CONSTEXPR variant_alternative_t<I, variant<T...>>& emplace( A&&... a )
    {
        variant_base::template emplace<I>( std::forward<A>(a)... );
        return _get_impl( mp11::mp_size_t<I>() );
    }

    template<std::size_t I, class V, class... A, class E = typename std::enable_if<std::is_constructible<mp11::mp_at_c<variant<T...>, I>, std::initializer_list<V>&, A&&...>::value>::type>
    BOOST_CXX14_CONSTEXPR variant_alternative_t<I, variant<T...>>& emplace( std::initializer_list<V> il, A&&... a )
    {
        variant_base::template emplace<I>( il, std::forward<A>(a)... );
        return _get_impl( mp11::mp_size_t<I>() );
    }

    // value status

    constexpr bool valueless_by_exception() const noexcept
    {
        return false;
    }

    using variant_base::index;

    // swap

private:

    struct L5
    {
        variant * this_;
        variant & r;

        template<class I> void operator()( I i ) const
        {
            using std::swap;
            swap( this_->_get_impl( i ), r._get_impl( i ) );
        }
    };

public:

    void swap( variant& r ) noexcept( mp11::mp_all<std::is_nothrow_move_constructible<T>..., detail::is_nothrow_swappable<T>...>::value )
    {
        if( index() == r.index() )
        {
            mp11::mp_with_index<sizeof...(T)>( index(), L5{ this, r } );
        }
        else
        {
            variant tmp( std::move(*this) );
            *this = std::move( r );
            r = std::move( tmp );
        }
    }

    // private accessors

    using variant_base::_get_impl;

    // converting constructors (extension)

private:

    template<class... U> struct L6
    {
        variant_base * this_;
        variant<U...> const & r;

        template<class I> void operator()( I i ) const
        {
            using J = mp11::mp_find<mp11::mp_list<T...>, mp11::mp_at<mp11::mp_list<U...>, I>>;
            this_->_replace( J{}, r._get_impl( i ) );
        }
    };

public:

    template<class... U,
        class E2 = mp11::mp_if<mp11::mp_all<std::is_copy_constructible<U>..., mp11::mp_contains<mp11::mp_list<T...>, U>...>, void> >
    variant( variant<U...> const& r )
        noexcept( mp11::mp_all<std::is_nothrow_copy_constructible<U>...>::value )
    {
        mp11::mp_with_index<sizeof...(U)>( r.index(), L6<U...>{ this, r } );
    }

private:

    template<class... U> struct L7
    {
        variant_base * this_;
        variant<U...> & r;

        template<class I> void operator()( I i ) const
        {
            using J = mp11::mp_find<mp11::mp_list<T...>, mp11::mp_at<mp11::mp_list<U...>, I>>;
            this_->_replace( J{}, std::move( r._get_impl( i ) ) );
        }
    };

public:

    template<class... U,
        class E2 = mp11::mp_if<mp11::mp_all<std::is_move_constructible<U>..., mp11::mp_contains<mp11::mp_list<T...>, U>...>, void> >
    variant( variant<U...> && r )
        noexcept( mp11::mp_all<std::is_nothrow_move_constructible<U>...>::value )
    {
        mp11::mp_with_index<sizeof...(U)>( r.index(), L7<U...>{ this, r } );
    }

    // subset (extension)

private:

    template<class... U, class V, std::size_t J, class E = typename std::enable_if<J != sizeof...(U)>::type> static constexpr variant<U...> _subset_impl( mp11::mp_size_t<J>, V && v )
    {
        return variant<U...>( in_place_index_t<J>(), std::forward<V>(v) );
    }

    template<class... U, class V> static variant<U...> _subset_impl( mp11::mp_size_t<sizeof...(U)>, V && /*v*/ )
    {
        detail::throw_bad_variant_access();
    }

private:

    template<class... U> struct L8
    {
        variant * this_;

        template<class I> variant<U...> operator()( I i ) const
        {
            using J = mp11::mp_find<mp11::mp_list<U...>, mp11::mp_at<mp11::mp_list<T...>, I>>;
            return this_->_subset_impl<U...>( J{}, this_->_get_impl( i ) );
        }
    };

public:

    template<class... U,
        class E2 = mp11::mp_if<mp11::mp_all<std::is_copy_constructible<U>..., mp11::mp_contains<mp11::mp_list<T...>, U>...>, void> >
    BOOST_CXX14_CONSTEXPR variant<U...> subset() &
    {
        return mp11::mp_with_index<sizeof...(T)>( index(), L8<U...>{ this } );
    }

private:

    template<class... U> struct L9
    {
        variant const * this_;

        template<class I> variant<U...> operator()( I i ) const
        {
            using J = mp11::mp_find<mp11::mp_list<U...>, mp11::mp_at<mp11::mp_list<T...>, I>>;
            return this_->_subset_impl<U...>( J{}, this_->_get_impl( i ) );
        }
    };

public:

    template<class... U,
        class E2 = mp11::mp_if<mp11::mp_all<std::is_copy_constructible<U>..., mp11::mp_contains<mp11::mp_list<T...>, U>...>, void> >
    constexpr variant<U...> subset() const&
    {
        return mp11::mp_with_index<sizeof...(T)>( index(), L9<U...>{ this } );
    }

private:

    template<class... U> struct L10
    {
        variant * this_;

        template<class I> variant<U...> operator()( I i ) const
        {
            using J = mp11::mp_find<mp11::mp_list<U...>, mp11::mp_at<mp11::mp_list<T...>, I>>;
            return this_->_subset_impl<U...>( J{}, std::move( this_->_get_impl( i ) ) );
        }
    };

public:

    template<class... U,
        class E2 = mp11::mp_if<mp11::mp_all<std::is_copy_constructible<U>..., mp11::mp_contains<mp11::mp_list<T...>, U>...>, void> >
    BOOST_CXX14_CONSTEXPR variant<U...> subset() &&
    {
        return mp11::mp_with_index<sizeof...(T)>( index(), L10<U...>{ this } );
    }

#if !BOOST_WORKAROUND(BOOST_GCC, < 40900)

    // g++ 4.8 doesn't handle const&& particularly well

private:

    template<class... U> struct L11
    {
        variant const * this_;

        template<class I> variant<U...> operator()( I i ) const
        {
            using J = mp11::mp_find<mp11::mp_list<U...>, mp11::mp_at<mp11::mp_list<T...>, I>>;
            return this_->_subset_impl<U...>( J{}, std::move( this_->_get_impl( i ) ) );
        }
    };

public:

    template<class... U,
        class E2 = mp11::mp_if<mp11::mp_all<std::is_copy_constructible<U>..., mp11::mp_contains<mp11::mp_list<T...>, U>...>, void> >
    constexpr variant<U...> subset() const&&
    {
        return mp11::mp_with_index<sizeof...(T)>( index(), L11<U...>{ this } );
    }

#endif
};

// relational operators

namespace detail
{

template<class... T> struct eq_L
{
    variant<T...> const & v;
    variant<T...> const & w;

    template<class I> constexpr bool operator()( I i ) const
    {
        return v._get_impl( i ) == w._get_impl( i );
    }
};

} // namespace detail

template<class... T> constexpr bool operator==( variant<T...> const & v, variant<T...> const & w )
{
    return v.index() == w.index() && mp11::mp_with_index<sizeof...(T)>( v.index(), detail::eq_L<T...>{ v, w } );
}

namespace detail
{

template<class... T> struct ne_L
{
    variant<T...> const & v;
    variant<T...> const & w;

    template<class I> constexpr bool operator()( I i ) const
    {
        return v._get_impl( i ) != w._get_impl( i );
    }
};

} // namespace detail

template<class... T> constexpr bool operator!=( variant<T...> const & v, variant<T...> const & w )
{
    return v.index() != w.index() || mp11::mp_with_index<sizeof...(T)>( v.index(), detail::ne_L<T...>{ v, w } );
}

namespace detail
{

template<class... T> struct lt_L
{
    variant<T...> const & v;
    variant<T...> const & w;

    template<class I> constexpr bool operator()( I i ) const
    {
        return v._get_impl( i ) < w._get_impl( i );
    }
};

} // namespace detail

template<class... T> constexpr bool operator<( variant<T...> const & v, variant<T...> const & w )
{
    return v.index() < w.index() || ( v.index() == w.index() && mp11::mp_with_index<sizeof...(T)>( v.index(), detail::lt_L<T...>{ v, w } ) );
}

template<class... T> constexpr bool operator>(  variant<T...> const & v, variant<T...> const & w )
{
    return w < v;
}

namespace detail
{

template<class... T> struct le_L
{
    variant<T...> const & v;
    variant<T...> const & w;

    template<class I> constexpr bool operator()( I i ) const
    {
        return v._get_impl( i ) <= w._get_impl( i );
    }
};

} // namespace detail

template<class... T> constexpr bool operator<=( variant<T...> const & v, variant<T...> const & w )
{
    return v.index() < w.index() || ( v.index() == w.index() && mp11::mp_with_index<sizeof...(T)>( v.index(), detail::le_L<T...>{ v, w } ) );
}

template<class... T> constexpr bool operator>=( variant<T...> const & v, variant<T...> const & w )
{
    return w <= v;
}

// visitation
namespace detail
{

template<class T> using remove_cv_ref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

template<class T, class U> struct copy_cv_ref
{
    using type = T;
};

template<class T, class U> struct copy_cv_ref<T, U const>
{
    using type = T const;
};

template<class T, class U> struct copy_cv_ref<T, U volatile>
{
    using type = T volatile;
};

template<class T, class U> struct copy_cv_ref<T, U const volatile>
{
    using type = T const volatile;
};

template<class T, class U> struct copy_cv_ref<T, U&>
{
    using type = typename copy_cv_ref<T, U>::type&;
};

template<class T, class U> struct copy_cv_ref<T, U&&>
{
    using type = typename copy_cv_ref<T, U>::type&&;
};

template<class T, class U> using copy_cv_ref_t = typename copy_cv_ref<T, U>::type;

template<class F> struct Qret
{
    template<class... T> using fn = decltype( std::declval<F>()( std::declval<T>()... ) );
};

template<class L> using front_if_same = mp11::mp_if<mp11::mp_apply<mp11::mp_same, L>, mp11::mp_front<L>>;

template<class V> using apply_cv_ref = mp11::mp_product<copy_cv_ref_t, remove_cv_ref_t<V>, mp11::mp_list<V>>;

template<class F, class... V> using Vret = front_if_same<mp11::mp_product_q<Qret<F>, apply_cv_ref<V>...>>;

} // namespace detail

template<class F> constexpr auto visit( F&& f ) -> decltype(std::forward<F>(f)())
{
    return std::forward<F>(f)();
}

namespace detail
{

template<class F, class V1> struct visit_L1
{
    F&& f;
    V1&& v1;

    template<class I> auto operator()( I ) const -> Vret<F, V1>
    {
        return std::forward<F>(f)( unsafe_get<I::value>( std::forward<V1>(v1) ) );
    }
};

} // namespace detail

template<class F, class V1> constexpr auto visit( F&& f, V1&& v1 ) -> detail::Vret<F, V1>
{
    return mp11::mp_with_index<variant_size<V1>>( v1.index(), detail::visit_L1<F, V1>{ std::forward<F>(f), std::forward<V1>(v1) } );
}

#if defined(BOOST_NO_CXX14_GENERIC_LAMBDAS) || BOOST_WORKAROUND( BOOST_MSVC, < 1920 )

namespace detail
{

template<class F, class A> struct bind_front_
{
    F&& f;
    A&& a;

    template<class... T> auto operator()( T&&... t ) -> decltype( std::forward<F>(f)( std::forward<A>(a), std::forward<T>(t)... ) )
    {
        return std::forward<F>(f)( std::forward<A>(a), std::forward<T>(t)... );
    }
};

template<class F, class A> bind_front_<F, A> bind_front( F&& f, A&& a )
{
    return bind_front_<F, A>{ std::forward<F>(f), std::forward<A>(a) };
}

template<class F, class V1, class V2> struct visit_L2
{
    F&& f;

    V1&& v1;
    V2&& v2;

    template<class I> auto operator()( I ) const -> Vret<F, V1, V2>
    {
        auto f2 = bind_front( std::forward<F>(f), unsafe_get<I::value>( std::forward<V1>(v1) ) );
        return visit( f2, std::forward<V2>(v2) );
    }
};

} // namespace detail

template<class F, class V1, class V2> constexpr auto visit( F&& f, V1&& v1, V2&& v2 ) -> detail::Vret<F, V1, V2>
{
    return mp11::mp_with_index<variant_size<V1>>( v1.index(), detail::visit_L2<F, V1, V2>{ std::forward<F>(f), std::forward<V1>(v1), std::forward<V2>(v2) } );
}

namespace detail
{

template<class F, class V1, class V2, class V3> struct visit_L3
{
    F&& f;

    V1&& v1;
    V2&& v2;
    V3&& v3;

    template<class I> auto operator()( I ) const -> Vret<F, V1, V2, V3>
    {
        auto f2 = bind_front( std::forward<F>(f), unsafe_get<I::value>( std::forward<V1>(v1) ) );
        return visit( f2, std::forward<V2>(v2), std::forward<V3>(v3) );
    }
};

} // namespace detail

template<class F, class V1, class V2, class V3> constexpr auto visit( F&& f, V1&& v1, V2&& v2, V3&& v3 ) -> detail::Vret<F, V1, V2, V3>
{
    return mp11::mp_with_index<variant_size<V1>>( v1.index(), detail::visit_L3<F, V1, V2, V3>{ std::forward<F>(f), std::forward<V1>(v1), std::forward<V2>(v2), std::forward<V3>(v3) } );
}

namespace detail
{

template<class F, class V1, class V2, class V3, class V4> struct visit_L4
{
    F&& f;

    V1&& v1;
    V2&& v2;
    V3&& v3;
    V4&& v4;

    template<class I> auto operator()( I ) const -> Vret<F, V1, V2, V3, V4>
    {
        auto f2 = bind_front( std::forward<F>(f), unsafe_get<I::value>( std::forward<V1>(v1) ) );
        return visit( f2, std::forward<V2>(v2), std::forward<V3>(v3), std::forward<V4>(v4) );
    }
};

} // namespace detail

template<class F, class V1, class V2, class V3, class V4> constexpr auto visit( F&& f, V1&& v1, V2&& v2, V3&& v3, V4&& v4 ) -> detail::Vret<F, V1, V2, V3, V4>
{
    return mp11::mp_with_index<variant_size<V1>>( v1.index(), detail::visit_L4<F, V1, V2, V3, V4>{ std::forward<F>(f), std::forward<V1>(v1), std::forward<V2>(v2), std::forward<V3>(v3), std::forward<V4>(v4) } );
}

#else

template<class F, class V1, class V2, class... V> constexpr auto visit( F&& f, V1&& v1, V2&& v2, V&&... v ) -> detail::Vret<F, V1, V2, V...>
{
    return mp11::mp_with_index<variant_size<V1>>( v1.index(), [&]( auto I ){

        auto f2 = [&]( auto&&... a ){ return std::forward<F>(f)( detail::unsafe_get<I.value>( std::forward<V1>(v1) ), std::forward<decltype(a)>(a)... ); };
        return visit( f2, std::forward<V2>(v2), std::forward<V>(v)... );

    });
}

#endif

// specialized algorithms
template<class... T,
    class E = typename std::enable_if<mp11::mp_all<std::is_move_constructible<T>..., detail::is_swappable<T>...>::value>::type>
void swap( variant<T...> & v, variant<T...> & w )
    noexcept( noexcept(v.swap(w)) )
{
    v.swap( w );
}

} // namespace variant2
} // namespace boost

#if defined(_MSC_VER) && _MSC_VER < 1910
# pragma warning( pop )
#endif

#endif // #ifndef BOOST_VARIANT2_VARIANT_HPP_INCLUDED
