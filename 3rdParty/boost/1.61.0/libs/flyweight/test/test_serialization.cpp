/* Boost.Flyweight test of serialization capabilities.
 *
 * Copyright 2006-2014 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/flyweight for library home page.
 */

#include "test_serialization.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/flyweight.hpp> 
#include "test_serialization_template.hpp"

using namespace boost::flyweights;

struct serialization_flyweight_specifier
{
  template<typename T>
  struct apply
  {
    typedef flyweight<T> type;
  };
};

void test_serialization()
{
  test_serialization_template<serialization_flyweight_specifier>();
}
