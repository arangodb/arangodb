// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined(ONLY_V2)
# define NO_BV
# define NO_SV
#endif

#if defined(ONLY_BV)
# define NO_V2
# define NO_SV
#endif

#if defined(ONLY_SV)
# define NO_V2
# define NO_BV
#endif

#if !defined(NO_V2)
#include <boost/variant2/variant.hpp>
#endif

#if !defined(NO_BV)
#include <boost/variant.hpp>
#endif

#if !defined(NO_SV)
#include <variant>
#endif

#include <type_traits>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <string>
#include <stdexcept>

template<class T> struct is_numeric: std::integral_constant<bool, std::is_integral<T>::value || std::is_floating_point<T>::value>
{
};

template<class T, class U> struct have_addition: std::integral_constant<bool, is_numeric<T>::value && is_numeric<U>::value>
{
};

template<class T, class U, class E = std::enable_if_t<have_addition<T, U>::value>> auto add( T const& t, U const& u )
{
    return t + u;
}

template<class T, class U, class E = std::enable_if_t<!have_addition<T, U>::value>> double add( T const& /*t*/, U const& /*u*/ )
{
    throw std::logic_error( "Invalid addition" );
}

inline double to_double( double const& v )
{
    return v;
}

#if !defined(NO_V2)

template<class... T> boost::variant2::variant<T...> operator+( boost::variant2::variant<T...> const& v1, boost::variant2::variant<T...> const& v2 )
{
    return visit( [&]( auto const& x1, auto const & x2 ) -> boost::variant2::variant<T...> { return add( x1, x2 ); }, v1, v2 );
}

template<class... T> double to_double( boost::variant2::variant<T...> const& v )
{
    return boost::variant2::get<double>( v );
}

#endif

#if !defined(NO_BV)

template<class... T> boost::variant<T...> operator+( boost::variant<T...> const& v1, boost::variant<T...> const& v2 )
{
    return boost::apply_visitor( [&]( auto const& x1, auto const & x2 ) -> boost::variant<T...> { return add( x1, x2 ); }, v1, v2 );
}

template<class... T> double to_double( boost::variant<T...> const& v )
{
    return boost::get<double>( v );
}

#endif

#if !defined(NO_SV)

template<class... T> std::variant<T...> operator+( std::variant<T...> const& v1, std::variant<T...> const& v2 )
{
    return visit( [&]( auto const& x1, auto const & x2 ) -> std::variant<T...> { return add( x1, x2 ); }, v1, v2 );
}

template<class... T> double to_double( std::variant<T...> const& v )
{
    return std::get<double>( v );
}

#endif

template<class V> void test_( long long N )
{
    std::vector<V> w;
    // lack of reserve is deliberate

    auto tp1 = std::chrono::high_resolution_clock::now();

    for( long long i = 0; i < N; ++i )
    {
        V v;

        if( i % 7 == 0 )
        {
            v = i / 7;
        }
        else
        {
            v = i / 7.0;
        }

        w.push_back( v );
    }

    V s = 0.0;

    for( long long i = 0; i < N; ++i )
    {
        s = s + w[ i ];
    }

    auto tp2 = std::chrono::high_resolution_clock::now();

    std::cout << std::setw( 6 ) << std::chrono::duration_cast<std::chrono::milliseconds>( tp2 - tp1 ).count() << " ms; S=" << to_double( s ) << "\n";
}

template<class... T> void test( long long N )
{
    std::cout << "N=" << N << ":\n";

    std::cout << "        double: "; test_<double>( N );
#if !defined(NO_V2)
    std::cout << "      variant2: "; test_<boost::variant2::variant<T...>>( N );
#endif
#if !defined(NO_BV)
    std::cout << "boost::variant: "; test_<boost::variant<T...>>( N );
#endif
#if !defined(NO_SV)
    std::cout << "  std::variant: "; test_<std::variant<T...>>( N );
#endif

    std::cout << '\n';
}

int main()
{
    long long const N = 100'000'000LL;

    test<long long, double>( N );
    test<std::nullptr_t, long long, double, std::string, std::vector<std::string>, std::map<std::string, std::string>>( N );
}
