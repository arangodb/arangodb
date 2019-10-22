#ifndef META_HS_EXCEPT_BUILDER_HPP
#define META_HS_EXCEPT_BUILDER_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <grammar.hpp>
#include <ast.hpp>
#include <bind.hpp>
#include <curry.hpp>

#include <boost/mpl/map.hpp>
#include <boost/mpl/insert.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/apply_wrap.hpp>

#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/preprocessor/tuple/eat.hpp>

typedef boost::mpl::map<> empty_environment;

template <class Env = empty_environment>
struct builder
{
  typedef builder type;

  template <class Name, class Value>
  struct import :
    builder<
      typename boost::mpl::insert<
        Env,
        boost::mpl::pair<Name, ast::value<Value> >
      >::type
    >
  {};

#ifdef IMPORT
#  error IMPORT already defined
#endif
#define IMPORT(z, n, unused) \
  template < \
    class Name, \
    template <BOOST_PP_ENUM(n, class BOOST_PP_TUPLE_EAT(3), ~)> class F \
  > \
  struct BOOST_PP_CAT(import, n) : \
    builder< \
      typename boost::mpl::insert< \
        Env, \
        boost::mpl::pair<Name, ast::value<BOOST_PP_CAT(curry, n)<F> > > \
      >::type \
    > \
  {};

BOOST_PP_REPEAT_FROM_TO(1, CURRY_MAX_ARGUMENT, IMPORT, ~)

#undef IMPORT

  template <class S>
  struct define :
    builder<
      typename boost::mpl::insert<
        Env,
        typename boost::mpl::apply_wrap1<grammar::def_parser, S>::type
      >::type
    >
  {};

  template <class S>
  struct get :
    bind<typename boost::mpl::at<Env, S>::type, Env, Env>::type::type
  {};
};

#endif

