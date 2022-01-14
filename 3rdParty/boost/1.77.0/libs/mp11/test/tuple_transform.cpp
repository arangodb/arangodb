
// Copyright 2020 Hans Dembinski.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/tuple.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <tuple>
#include <utility>
#include <iosfwd>

// family of test types with state
template <int N>
struct T {
    int value;
    T() : value{N} {};
    explicit T(int n) : value{n} {}
};

template <int N>
std::ostream& operator<<( std::ostream& os, T<N> const& t )
{
    os << t.value;
    return os;
}

// test function changes type and value
struct F {
    template<int N, int M=1> T<N+M> operator()( T<N> a, T<M> b={} ) const
    {
        return T<N+M>{a.value + b.value + 1};
    }
};

int main()
{
    using boost::mp11::tuple_transform;

    {
        std::tuple<T<1>, T<2>, T<3>> tp;
        std::tuple<T<4>, T<5>, T<6>> tp2;

        {
            std::tuple<T<2>, T<3>, T<4>> s = tuple_transform( F{}, tp );
            BOOST_TEST_EQ( std::get<0>(s).value, 3 );
            BOOST_TEST_EQ( std::get<1>(s).value, 4 );
            BOOST_TEST_EQ( std::get<2>(s).value, 5 );
        }

        {
            std::tuple<T<2>, T<3>, T<4>> s = tuple_transform( F{}, std::move(tp) );
            BOOST_TEST_EQ( std::get<0>(s).value, 3 );
            BOOST_TEST_EQ( std::get<1>(s).value, 4 );
            BOOST_TEST_EQ( std::get<2>(s).value, 5 );
        }

        {
            std::tuple<T<5>, T<7>, T<9>> s = tuple_transform( F{}, tp, tp2 );
            BOOST_TEST_EQ( std::get<0>(s).value, 6 );
            BOOST_TEST_EQ( std::get<1>(s).value, 8 );
            BOOST_TEST_EQ( std::get<2>(s).value, 10 );
        }

        {
            std::tuple<T<5>, T<7>, T<9>> s = tuple_transform(
                F{}, std::move(tp), std::move(tp2)
            );
            BOOST_TEST_EQ( std::get<0>(s).value, 6 );
            BOOST_TEST_EQ( std::get<1>(s).value, 8 );
            BOOST_TEST_EQ( std::get<2>(s).value, 10 );
        }
    }

    {
        std::tuple<T<1>, T<2>, T<3>> const tp;
        std::tuple<T<4>, T<5>, T<6>> const tp2;

        {
            std::tuple<T<2>, T<3>, T<4>> s = tuple_transform( F{}, tp );
            BOOST_TEST_EQ( std::get<0>(s).value, 3 );
            BOOST_TEST_EQ( std::get<1>(s).value, 4 );
            BOOST_TEST_EQ( std::get<2>(s).value, 5 );
        }

        {
            std::tuple<T<2>, T<3>, T<4>> s = tuple_transform( F{}, std::move(tp) );
            BOOST_TEST_EQ( std::get<0>(s).value, 3 );
            BOOST_TEST_EQ( std::get<1>(s).value, 4 );
            BOOST_TEST_EQ( std::get<2>(s).value, 5 );
        }

        {
            std::tuple<T<5>, T<7>, T<9>> s = tuple_transform( F{}, tp, tp2 );
            BOOST_TEST_EQ( std::get<0>(s).value, 6 );
            BOOST_TEST_EQ( std::get<1>(s).value, 8 );
            BOOST_TEST_EQ( std::get<2>(s).value, 10 );
        }

        {
            std::tuple<T<5>, T<7>, T<9>> s = tuple_transform(
                F{}, std::move(tp), std::move(tp2)
            );
            BOOST_TEST_EQ( std::get<0>(s).value, 6 );
            BOOST_TEST_EQ( std::get<1>(s).value, 8 );
            BOOST_TEST_EQ( std::get<2>(s).value, 10 );
        }
    }

    {
        std::pair<T<1>, T<2>> tp;
        std::pair<T<3>, T<4>> tp2;

        {
            std::tuple<T<2>, T<3>> s = tuple_transform( F{}, tp );
            BOOST_TEST_EQ( std::get<0>(s).value, 3 );
            BOOST_TEST_EQ( std::get<1>(s).value, 4 );
        }

        {
            std::tuple<T<2>, T<3>> s = tuple_transform( F{}, std::move(tp) );
            BOOST_TEST_EQ( std::get<0>(s).value, 3 );
            BOOST_TEST_EQ( std::get<1>(s).value, 4 );
        }

        {
            std::tuple<T<4>, T<6>> s = tuple_transform( F{}, tp, tp2 );
            BOOST_TEST_EQ( std::get<0>(s).value, 5 );
            BOOST_TEST_EQ( std::get<1>(s).value, 7 );
        }

        {
            std::tuple<T<4>, T<6>> s = tuple_transform(
                F{}, std::move(tp), std::move(tp2)
            );
            BOOST_TEST_EQ( std::get<0>(s).value, 5 );
            BOOST_TEST_EQ( std::get<1>(s).value, 7 );
        }
    }

    {
        std::pair<T<1>, T<2>> const tp;
        std::pair<T<3>, T<4>> const tp2;

        {
            std::tuple<T<2>, T<3>> s = tuple_transform( F{}, tp );
            BOOST_TEST_EQ( std::get<0>(s).value, 3 );
            BOOST_TEST_EQ( std::get<1>(s).value, 4 );
        }

        {
            std::tuple<T<2>, T<3>> s = tuple_transform( F{}, std::move(tp) );
            BOOST_TEST_EQ( std::get<0>(s).value, 3 );
            BOOST_TEST_EQ( std::get<1>(s).value, 4 );
        }

        {
            std::tuple<T<4>, T<6>> s = tuple_transform( F{}, tp, tp2 );
            BOOST_TEST_EQ( std::get<0>(s).value, 5 );
            BOOST_TEST_EQ( std::get<1>(s).value, 7 );
        }

        {
            std::tuple<T<4>, T<6>> s = tuple_transform(
                F{}, std::move(tp), std::move(tp2)
            );
            BOOST_TEST_EQ( std::get<0>(s).value, 5 );
            BOOST_TEST_EQ( std::get<1>(s).value, 7 );
        }
    }

    {
        std::tuple<> tp;

        {
            auto s = tuple_transform( F{}, tp );
            BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(s), std::tuple<>>));
        }

        {
            auto s = tuple_transform( F{}, std::move(tp) );
            BOOST_TEST_TRAIT_TRUE((std::is_same<decltype(s), std::tuple<>>));
        }
    }

    return boost::report_errors();
}
