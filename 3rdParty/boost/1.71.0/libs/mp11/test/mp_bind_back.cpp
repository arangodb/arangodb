
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/bind.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

template<class...> struct L {};
template<class, class> struct P {};

template<class T, class U> using is_base_of_t = typename std::is_base_of<T, U>::type;

struct B1 {};
struct B2 {};
struct D: B1, B2 {};
struct NB {};

int main()
{
    using namespace boost::mp11;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_back<L, char[1], char[2]>::fn<int[1], int[2], int[3]>, L<int[1], int[2], int[3], char[1], char[2]>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_back_q<mp_quote<L>, char[1], char[2]>::fn<int[1], int[2], int[3]>, L<int[1], int[2], int[3], char[1], char[2]>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_back<P, char[1]>::fn<int[1]>, P<int[1], char[1]>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_back_q<mp_quote<P>, char[1]>::fn<int[1]>, P<int[1], char[1]>>));

    //

    using L1 = L<B1, B2, NB>;

    {
        using L2 = mp_transform<mp_bind_back<is_base_of_t, D>::fn, L1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<L2, L<std::true_type, std::true_type, std::false_type>>));
    }

    {
        using L2 = mp_transform_q<mp_bind_back<is_base_of_t, D>, L1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<L2, L<std::true_type, std::true_type, std::false_type>>));
    }

    {
        using L2 = mp_transform<mp_bind_back_q<mp_quote<is_base_of_t>, D>::fn, L1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<L2, L<std::true_type, std::true_type, std::false_type>>));
    }

    {
        using L2 = mp_transform_q<mp_bind_back_q<mp_quote<is_base_of_t>, D>, L1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<L2, L<std::true_type, std::true_type, std::false_type>>));
    }

    //

    return boost::report_errors();
}
