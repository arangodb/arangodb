/*
  [auto_generated]
  libs/numeric/odeint/test/dummy_observers.hpp

  [begin_description]
  tba.
  [end_description]

  Copyright 2013 Karsten Ahnert
  Copyright 2013 Mario Mulansky

  Distributed under the Boost Software License, Version 1.0.
  (See accompanying file LICENSE_1_0.txt or
  copy at http://www.boost.org/LICENSE_1_0.txt)
*/


#ifndef LIBS_NUMERIC_ODEINT_TEST_DUMMY_OBSERVERS_HPP_DEFINED
#define LIBS_NUMERIC_ODEINT_TEST_DUMMY_OBSERVERS_HPP_DEFINED


namespace boost {
namespace numeric {
namespace odeint {


struct dummy_observer
{
    template< class State >
    void operator()( const State &s ) const
    {
    }
};


} // namespace odeint
} // namespace numeric
} // namespace boost


#endif // LIBS_NUMERIC_ODEINT_TEST_DUMMY_OBSERVERS_HPP_DEFINED
