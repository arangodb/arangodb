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
#include <boost/type_erasure/builtin.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>

using namespace boost::type_erasure;
namespace mpl = boost::mpl;

template<class T = _self>
struct common : mpl::vector<
    copy_constructible<T>
> {};

template<class T = _self>
struct concept1 : mpl::vector<copy_constructible<T> > {};

template<class T = _self>
struct concept2 : mpl::vector<concept1<T> > {};

namespace boost {
namespace type_erasure {

template<class T, class Base>
struct concept_interface<concept1<T>, Base, T> : Base
{
    typedef int id_type;
};

template<class T, class Base>
struct concept_interface<concept2<T>, Base, T> : Base
{
    typedef char id_type;
};

}
}

BOOST_MPL_ASSERT((boost::is_same<any<concept2<> >::id_type, char>));
BOOST_MPL_ASSERT((boost::is_same<any<mpl::vector<concept2<>, concept1<> > >::id_type, char>));
BOOST_MPL_ASSERT((boost::is_same<any<mpl::vector<concept1<>, concept2<> > >::id_type, char>));
