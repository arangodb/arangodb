
// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning( disable: 4503 ) // decorated name length exceeded
#endif

#include <boost/variant2/variant.hpp>
#include <boost/mp11.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::mp11;

template<class I> struct X
{
    static int const value = I::value;
    int v_;
};

template<class I> int const X<I>::value;

template<class I> struct Y
{
    static int const value = I::value;
    int v_;

    Y() = default;
    Y( Y const& ) = default;

    explicit Y( int v ): v_( v ) {}

    Y& operator=( Y const& ) noexcept = default;

    Y& operator=( Y&& r ) noexcept
    {
        v_ = r.v_;
        return *this;
    }
};

template<class I> int const Y<I>::value;

template<class I> struct Z
{
    static int const value = I::value;
    int v_;

    ~Z() {}
};

template<class I> int const Z<I>::value;

template<class V> struct F1
{
    template<class T> void operator()( T ) const
    {
        int const i = T::value;

        T t{ i * 2 };

        using boost::variant2::get;

        {
            V v( t );

            BOOST_TEST_EQ( v.index(), i );
            BOOST_TEST_EQ( get<i>( v ).v_, t.v_ );
            BOOST_TEST_EQ( get<T>( v ).v_, t.v_ );
        }

        {
            V const v( t );

            BOOST_TEST_EQ( v.index(), i );
            BOOST_TEST_EQ( get<i>( v ).v_, t.v_ );
            BOOST_TEST_EQ( get<T>( v ).v_, t.v_ );
        }

        {
            V v;

            v = t;

            BOOST_TEST_EQ( v.index(), i );
            BOOST_TEST_EQ( get<i>( v ).v_, t.v_ );
            BOOST_TEST_EQ( get<T>( v ).v_, t.v_ );
        }
    }
};

template<class V> void test()
{
    mp_for_each<V>( F1<V>() );
}

int main()
{
    int const N = 32;

    using V = mp_rename<mp_iota_c<N>, boost::variant2::variant>;

    test< mp_transform<X, V> >();
    test< mp_transform<Y, V> >();
    test< mp_transform<Z, V> >();

    return boost::report_errors();
}
