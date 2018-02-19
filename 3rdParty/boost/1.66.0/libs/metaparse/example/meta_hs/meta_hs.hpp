#ifndef META_HS_META_HS_HPP
#define META_HS_META_HS_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <builder.hpp>

#include <boost/mpl/plus.hpp>
#include <boost/mpl/minus.hpp>
#include <boost/mpl/times.hpp>
#include <boost/mpl/divides.hpp>
#include <boost/mpl/less.hpp>
#include <boost/mpl/less_equal.hpp>
#include <boost/mpl/greater.hpp>
#include <boost/mpl/greater_equal.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/not_equal_to.hpp>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

#ifdef DEFINE_LAZY
  #error DEFINE_LAZY already defined
#endif
#define DEFINE_LAZY(r, unused, name) \
  template <class A, class B> \
  struct BOOST_PP_CAT(lazy_, name) : \
    boost::mpl::name<typename A::type, typename B::type> \
  {};

BOOST_PP_SEQ_FOR_EACH(DEFINE_LAZY, ~,
  (plus)
  (minus)
  (times)
  (divides)
  (less)
  (less_equal)
  (greater)
  (greater_equal)
  (equal_to)
  (not_equal_to)
)

#undef DEFINE_LAZY

typedef builder<>
  ::import2<boost::metaparse::string<'.','+','.'>, lazy_plus>::type
  ::import2<boost::metaparse::string<'.','-','.'>, lazy_minus>::type
  ::import2<boost::metaparse::string<'.','*','.'>, lazy_times>::type
  ::import2<boost::metaparse::string<'.','/','.'>, lazy_divides>::type
  ::import2<boost::metaparse::string<'.','<','.'>, lazy_less>::type
  ::import2<boost::metaparse::string<'.','<','=','.'>, lazy_less_equal>::type
  ::import2<boost::metaparse::string<'.','>','.'>, lazy_greater>::type
  ::import2<boost::metaparse::string<'.','>','=','.'>, lazy_greater_equal>::type
  ::import2<boost::metaparse::string<'.','=','=','.'>, lazy_equal_to>::type
  ::import2<boost::metaparse::string<'.','/','=','.'>, lazy_not_equal_to>::type

  meta_hs;

#endif

