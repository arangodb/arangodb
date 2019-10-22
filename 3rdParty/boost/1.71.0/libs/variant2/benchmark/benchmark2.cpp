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

struct prefix
{
    int v_;
};

struct X1: prefix {};
struct X2: prefix {};
struct X3: prefix {};
struct X4: prefix {};
struct X5: prefix {};
struct X6: prefix {};
struct X7: prefix {};
struct X8: prefix {};
struct X9: prefix {};
struct X10: prefix {};
struct X11: prefix {};
struct X12: prefix {};

inline int get_value( prefix const& v )
{
    return v.v_;
}

#if !defined(NO_V2)

template<class... T> int get_value( boost::variant2::variant<T...> const& v )
{
    return visit( []( prefix const& x ) { return x.v_; }, v );
}

#endif

#if !defined(NO_BV)

template<class... T> int get_value( boost::variant<T...> const& v )
{
    return boost::apply_visitor( []( prefix const& x ) { return x.v_; }, v );
}

#endif

#if !defined(NO_SV)

template<class... T> int get_value( std::variant<T...> const& v )
{
    return visit( []( prefix const& x ) { return x.v_; }, v );
}

#endif

template<class V> void test_( int N )
{
    std::vector<V> w;
    // lack of reserve is deliberate

    auto tp1 = std::chrono::high_resolution_clock::now();

    for( int i = 0; i < N / 12; ++i )
    {
        w.push_back( X1{ i } );
        w.push_back( X2{ i } );
        w.push_back( X3{ i } );
        w.push_back( X4{ i } );
        w.push_back( X5{ i } );
        w.push_back( X6{ i } );
        w.push_back( X7{ i } );
        w.push_back( X8{ i } );
        w.push_back( X9{ i } );
        w.push_back( X10{ i } );
        w.push_back( X11{ i } );
        w.push_back( X12{ i } );
    }

    unsigned long long s = 0;

    for( std::size_t i = 0, n = w.size(); i < n; ++i )
    {
        s = s + get_value( w[ i ] );
    }

    auto tp2 = std::chrono::high_resolution_clock::now();

    std::cout << std::setw( 6 ) << std::chrono::duration_cast<std::chrono::milliseconds>( tp2 - tp1 ).count() << " ms; S=" << s << "\n";
}

template<class... T> void test( int N )
{
    std::cout << "N=" << N << ":\n";

    std::cout << "        prefix: "; test_<prefix>( N );
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
    int const N = 100'000'000;

    test<X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12>( N );
}
