// Boost.TypeErasure library
//
// Copyright 2011 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// $Id$

#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/free.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/back_inserter.hpp>
#include <boost/mpl/range_c.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

using namespace boost::type_erasure;
namespace mpl = boost::mpl;

template<int N>
struct tester
{
    int value;
};

int func() { return 0; }

template<int N, class... T>
int func(tester<N> t0, T... t)
{
    return t0.value + func(t...);
}

BOOST_TYPE_ERASURE_FREE((has_func), func)

template<class T = _self>
struct common : mpl::vector<
    copy_constructible<T>
> {};

BOOST_AUTO_TEST_CASE(test_arity)
{
    tester<0> t = { 1 };
    any<mpl::vector<common<>, has_func<int(_self, _self, _self, _self, _self, _self)> > > x(t);
    int i = func(x, x, x, x, x, x);
    BOOST_TEST(i == 6);
}

BOOST_AUTO_TEST_CASE(test_null_arity)
{
    any<mpl::vector<common<>, has_func<int(_self, _self, _self, _self, _self, _self)>, relaxed> > x;
    BOOST_CHECK_THROW(func(x, x, x, x, x , x), boost::type_erasure::bad_function_call);
}

template<class T0, class... T>
struct my_concept
{
    static int apply(T0 t0) { return func(t0); }
};

BOOST_AUTO_TEST_CASE(test_template_arity)
{
    typedef my_concept<_self, int, int, int, int, int, int> concept1;
    tester<0> t = { 1 };
    any<mpl::vector<common<>, concept1> > x(t);
    int i = call(concept1(), x);
    BOOST_TEST(i == 1);
}

template<class T>
struct make_funcN
{
    typedef has_func<int(_self, tester<T::value>)> type;
};

BOOST_AUTO_TEST_CASE(test_vtable_size)
{
    tester<0> t = { 1 };
    any<mpl::vector<
        common<>,
        mpl::transform<mpl::range_c<int, 1, 60>,
            make_funcN<mpl::_1>,
            mpl::back_inserter< boost::mp11::mp_list<> >
        >::type
    > > x(t);
    tester<7> t1 = { 2 };
    int i = func(x, t1);
    BOOST_TEST(i == 3);
}
