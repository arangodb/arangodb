//  scoped_enum_emulation_test.cpp  ----------------------------------------------------//

//  Copyright Beman Dawes, 2009

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  See documentation at http://www.boost.org/libs/utility/scoped_enum_emulation.html

// #include <boost/detail/disable_warnings.hpp>
// #include <boost/config/warning_disable.hpp>


#include <boost/detail/scoped_enum_emulation.hpp>
#include <boost/assert.hpp>

BOOST_SCOPED_ENUM_START(traffic_light) { red=0, yellow, green }; BOOST_SCOPED_ENUM_END

BOOST_SCOPED_ENUM_START(algae) { green=0, red, cyan }; BOOST_SCOPED_ENUM_END

struct color
{
  BOOST_SCOPED_ENUM_START(value_t) { red, green, blue }; BOOST_SCOPED_ENUM_END
  BOOST_SCOPED_ENUM(value_t) value;
};

void foo( BOOST_SCOPED_ENUM(algae) arg )
{
  BOOST_ASSERT( arg == algae::cyan );
}

int main()
{
  BOOST_SCOPED_ENUM(traffic_light) signal( traffic_light::red );
  BOOST_SCOPED_ENUM(algae) sample( algae::red );

  BOOST_ASSERT( signal == traffic_light::red );
  BOOST_ASSERT( sample == algae::red );

  foo( algae::cyan );

  color tracker;
  tracker.value = color::value_t::blue;

  if (tracker.value  == color::value_t::blue) return 0; // quiet warnings

  return 0;
}
