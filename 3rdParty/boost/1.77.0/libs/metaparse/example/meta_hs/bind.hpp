#ifndef META_HS_BIND_HPP
#define META_HS_BIND_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <lazy.hpp>
#include <ast.hpp>

#include <boost/mpl/at.hpp>
#include <boost/mpl/insert.hpp>
#include <boost/mpl/pair.hpp>

template <class AST, class TopEnv, class Env>
struct bind;

template <class V, class TopEnv, class Env>
struct bind<ast::value<V>, TopEnv, Env>
{
  typedef lazy::value<V> type;
};

template <class Name, class TopEnv, class Env>
struct bind<ast::ref<Name>, TopEnv, Env> :
  bind<typename boost::mpl::at<Env, Name>::type, TopEnv, Env>
{};

template <class F, class Arg, class TopEnv, class Env>
struct bind<ast::application<F, Arg>, TopEnv, Env> 
{
  typedef
    lazy::application<
      typename bind<F, TopEnv, Env>::type,
      typename bind<Arg, TopEnv, Env>::type
    >
    type;
};

template <class F, class ArgName, class TopEnv, class Env>
struct bind<ast::lambda<F, ArgName>, TopEnv, Env>
{
  typedef bind type;
  
  template <class ArgValue>
  struct apply :
    bind<
      F,
      TopEnv,
      typename boost::mpl::insert<
        Env,
        boost::mpl::pair<ArgName, ast::value<ArgValue> >
      >::type
    >::type
  {};
};

template <class E, class TopEnv, class Env>
struct bind<ast::top_bound<E>, TopEnv, Env> : bind<E, TopEnv, TopEnv> {};

#endif

