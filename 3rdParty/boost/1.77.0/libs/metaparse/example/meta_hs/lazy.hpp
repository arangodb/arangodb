#ifndef META_HS_LAZY_HPP
#define META_HS_LAZY_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/apply_wrap.hpp>

namespace lazy
{
  template <class V>
  struct value
  {
    typedef V type;
  };

  template <class F, class Arg>
  struct application : boost::mpl::apply_wrap1<typename F::type, Arg>::type {};
}

#endif

