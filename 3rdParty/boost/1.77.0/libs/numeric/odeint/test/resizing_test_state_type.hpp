/*
  [auto_generated]
  libs/numeric/odeint/test/resizing_test_state_type.hpp

  [begin_description]
  tba.
  [end_description]

  Copyright 2009-2012 Karsten Ahnert
  Copyright 2009-2012 Mario Mulansky

  Distributed under the Boost Software License, Version 1.0.
  (See accompanying file LICENSE_1_0.txt or
  copy at http://www.boost.org/LICENSE_1_0.txt)
*/


#ifndef LIBS_NUMERIC_ODEINT_TEST_RESIZING_TEST_STATE_TYPE_HPP_DEFINED
#define LIBS_NUMERIC_ODEINT_TEST_RESIZING_TEST_STATE_TYPE_HPP_DEFINED

#include <boost/numeric/odeint/util/is_resizeable.hpp>
#include <boost/numeric/odeint/util/resize.hpp>
#include <boost/numeric/odeint/util/same_size.hpp>

#include <boost/array.hpp>



// Mario: velocity verlet tests need arrays of size 2
// some ugly detailed dependency, maybe this can be improved?
class test_array_type : public boost::array< double , 2 > { };

size_t adjust_size_count;

namespace boost {
namespace numeric {
namespace odeint {

    template<>
    struct is_resizeable< test_array_type >
    {
        typedef boost::true_type type;
        const static bool value = type::value;
    };

    template<>
    struct same_size_impl< test_array_type , test_array_type >
    {
        static bool same_size( const test_array_type &x1 , const test_array_type &x2 )
        {
            return false;
        }
    };

    template<>
    struct resize_impl< test_array_type , test_array_type >
    {
        static void resize( test_array_type &x1 , const test_array_type &x2 )
        {
            adjust_size_count++;
        }
    };

} // namespace odeint
} // namespace numeric
} // namespace boost


#endif // LIBS_NUMERIC_ODEINT_TEST_RESIZING_TEST_STATE_TYPE_HPP_DEFINED
