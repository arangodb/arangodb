#ifndef DOUBLE_NUMBER_HPP
#define DOUBLE_NUMBER_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/int.hpp>
#include <boost/mpl/times.hpp>

template <class N>
struct double_number :
  boost::mpl::times<typename N::type, boost::mpl::int_<2> >
{};

#endif

