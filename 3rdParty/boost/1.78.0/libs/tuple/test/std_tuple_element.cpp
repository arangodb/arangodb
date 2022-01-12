// Copyright 2017 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/tuple/tuple.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX11_HDR_TUPLE)

BOOST_PRAGMA_MESSAGE("Skipping std::tuple_element tests for lack of <tuple>")
int main() {}

#else

#include <tuple>

template<class Tp, std::size_t I, class E> void test()
{
    BOOST_TEST_TRAIT_TRUE((boost::is_same<typename std::tuple_element<I, Tp>::type, E>));

    typedef typename Tp::inherited Tp2;
    BOOST_TEST_TRAIT_TRUE((boost::is_same<typename std::tuple_element<I, Tp2>::type, E>));
}

template<int> struct X
{
};

int main()
{
    test<boost::tuple<X<0> const>, 0, X<0> const>();

    test<boost::tuple<X<0> const, X<1> const>, 0, X<0> const>();
    test<boost::tuple<X<0> const, X<1> const>, 1, X<1> const>();

    test<boost::tuple<X<0> const, X<1> const, X<2> const>, 0, X<0> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const>, 1, X<1> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const>, 2, X<2> const>();

    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const>, 0, X<0> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const>, 1, X<1> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const>, 2, X<2> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const>, 3, X<3> const>();

    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const, X<4> const>, 0, X<0> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const, X<4> const>, 1, X<1> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const, X<4> const>, 2, X<2> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const, X<4> const>, 3, X<3> const>();
    test<boost::tuple<X<0> const, X<1> const, X<2> const, X<3> const, X<4> const>, 4, X<4> const>();

    return boost::report_errors();
}

#endif
