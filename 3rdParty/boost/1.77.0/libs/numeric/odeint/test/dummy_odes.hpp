/*
  [auto_generated]
  libs/numeric/odeint/test/dummy_odes.hpp

  [begin_description]
  tba.
  [end_description]

  Copyright 2012-2013 Karsten Ahnert
  Copyright 2013 Mario Mulansky

  Distributed under the Boost Software License, Version 1.0.
  (See accompanying file LICENSE_1_0.txt or
  copy at http://www.boost.org/LICENSE_1_0.txt)
*/


#ifndef LIBS_NUMERIC_ODEINT_TEST_DUMMY_ODES_HPP_DEFINED
#define LIBS_NUMERIC_ODEINT_TEST_DUMMY_ODES_HPP_DEFINED

#include <boost/fusion/include/at_c.hpp>






/*
 * rhs functors/functions for different state types
 */
struct constant_system_functor_standard
{
    template< class State , class Deriv , class Time >
    void operator()( const State &x , Deriv &dxdt , const Time t ) const
    {
        dxdt[0] = 1.0;
    }
};

struct constant_system_functor_vector_space
{
    template< class State , class Deriv , class Time >
    void operator()( const State &x , Deriv &dxdt , const Time t  ) const
    {
        dxdt = 1.0;
    }
};

struct constant_system_functor_fusion
{
    template< class State , class Deriv , class Time >
    void operator()( const State &x , Deriv &dxdt , const Time t ) const
    {
        boost::fusion::at_c< 0 >( dxdt ) = boost::fusion::at_c< 0 >( x ) / Time( 1.0 );
    }
};

struct lorenz
{
    template< typename State , typename Deriv , typename Time >
    void operator()( const State& x , Deriv& dxdt , const Time& t ) const
    {
        const Time sigma = 10.0;
        const Time R = 28.0;
        const Time b = 8.0 / 3.0;
        dxdt[0] = sigma * ( x[1] - x[0] );
        dxdt[1] = R * x[0] - x[1] - x[0] * x[2];
        dxdt[2] = -b * x[2] + x[0] * x[1];
    }
};

template< class State , class Deriv , class Time >
void constant_system_standard( const State &x , Deriv &dxdt , const Time t )
{
    dxdt[0] = 1.0;
}

template< class State , class Deriv , class Time >
void constant_system_vector_space( const State &x , Deriv &dxdt , const Time t ) 
{
    dxdt = 1.0;
}

template< class State , class Deriv , class Time >
void constant_system_fusion( const State &x , Deriv &dxdt , const Time t ) 
{
    boost::fusion::at_c< 0 >( dxdt ) = boost::fusion::at_c< 0 >( x ) / Time( 1.0 );
}




/*
 * rhs functors for symplectic steppers
 */
struct constant_mom_func
{
    template< class StateIn , class StateOut >
    void operator()( const StateIn &q , StateOut &dp ) const
    {
        dp[0] = 1.0;
    }
};

struct default_coor_func
{
    template< class StateIn , class StateOut >
    void operator()( const StateIn &p , StateOut &dq ) const
    {
        dq[0] = p[0];
    }
};



struct constant_mom_func_vector_space_1d
{
    template< class T >
    void operator()( const T &q , T &dp ) const
    {
        dp = 1.0;
    }
};

struct default_coor_func_vector_space_1d
{
    template< class T >
    void operator()( const T &p , T &dq ) const
    {
        dq = p;
    }
};







struct empty_system
{
    template <class State >
    void operator()( const State &x , State &dxdt , double t ) const
    {
    }
};




#endif // LIBS_NUMERIC_ODEINT_TEST_DUMMY_ODES_HPP_DEFINED
