#ifndef EXAMPLE_HANDCRAFTED_HPP
#define EXAMPLE_HANDCRAFTED_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <double_number.hpp>

#include <boost/mpl/int.hpp>
#include <boost/mpl/times.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/minus.hpp>
#include <boost/mpl/plus.hpp>
#include <boost/mpl/less.hpp>

typedef boost::mpl::int_<11> val;

struct fib
{
  typedef fib type;

  template <class N>
  struct impl;

  template <class N>
  struct apply :
    boost::mpl::eval_if<
      typename boost::mpl::less<N, boost::mpl::int_<2> >::type,
      boost::mpl::int_<1>,
      impl<N>
    >
  {};
};

template <class N>
struct fib::impl :
  boost::mpl::plus<
    typename fib::apply<
      typename boost::mpl::minus<N, boost::mpl::int_<1> >::type
    >::type,
    typename fib::apply<
      typename boost::mpl::minus<N, boost::mpl::int_<2> >::type
    >::type
  >
{};

struct fact
{
  typedef fact type;

  template <class N>
  struct impl;

  template <class N>
  struct apply :
    boost::mpl::eval_if<
      typename boost::mpl::less<N, boost::mpl::int_<1> >::type,
      boost::mpl::int_<1>,
      impl<N>
    >
  {};
};

template <class N>
struct fact::impl :
  boost::mpl::times<
    N,
    typename fact::apply<
      typename boost::mpl::minus<N, boost::mpl::int_<1> >::type
    >::type
  >
{};

struct times4
{
  typedef times4 type;

  template <class N>
  struct apply : double_number<typename double_number<N>::type> {};
};

struct times11
{
  typedef times11 type;

  template <class N>
  struct apply : boost::mpl::times<N, val> {};
};

#endif

