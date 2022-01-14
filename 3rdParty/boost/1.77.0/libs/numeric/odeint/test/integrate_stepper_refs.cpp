/*
 [auto_generated]
 libs/numeric/odeint/test/integrate_stepper_refs.cpp

 [begin_description]
 Tests the integrate functions with boost::ref( stepper)
 [end_description]

 Copyright 2009-2013 Karsten Ahnert
 Copyright 2009-2013 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */


#define BOOST_TEST_MODULE odeint_integrate_stepper_refs

#include <vector>
#include <cmath>
#include <iostream>

#include <boost/numeric/odeint/config.hpp>

#include <boost/noncopyable.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/iterator/counting_iterator.hpp>

#include <boost/mpl/vector.hpp>

#include <boost/numeric/odeint/integrate/integrate_const.hpp>
#include <boost/numeric/odeint/integrate/integrate_adaptive.hpp>
#include <boost/numeric/odeint/integrate/integrate_times.hpp>
#include <boost/numeric/odeint/integrate/integrate_n_steps.hpp>
#include <boost/numeric/odeint/stepper/controlled_step_result.hpp>


using namespace boost::unit_test;
using namespace boost::numeric::odeint;
namespace mpl = boost::mpl;

typedef double value_type;
typedef std::vector< value_type > state_type;

// minimal non-copyable basic stepper
template<
class State
>
class simple_stepper_nc : boost::noncopyable
{
public :
    typedef State state_type;
    typedef double value_type;
    typedef state_type deriv_type;
    typedef double time_type;
    typedef size_t order_type;
    typedef stepper_tag stepper_category;

    template< class System  >
    void do_step( System system , state_type &in , time_type t , time_type dt )
    {
        // empty

    }
};

// minimal non-copyable controlled stepper
template<
class State
>
class controlled_stepper_nc : boost::noncopyable
{
public :
    typedef State state_type;
    typedef double value_type;
    typedef state_type deriv_type;
    typedef double time_type;
    typedef size_t order_type;
    typedef controlled_stepper_tag stepper_category;

    template< class System  >
    controlled_step_result try_step( System system , state_type &in , time_type &t , time_type &dt )
    {
        std::cout << "dense out stepper: " << t << " , " << dt << std::endl;
        t += dt;
        return success;
    }
};

// minimal non-copyable dense_output stepper
template<
class State
>
class dense_out_stepper_nc : boost::noncopyable
{
public :
    typedef State state_type;
    typedef double value_type;
    typedef state_type deriv_type;
    typedef double time_type;
    typedef size_t order_type;
    typedef dense_output_stepper_tag stepper_category;

    void initialize( const state_type &x0 , const time_type t0 , const time_type dt0 )
    {
        m_x = x0;
        m_t = t0;
        m_dt = dt0;
        std::cout << "initialize: " << m_t << " , " << m_dt << std::endl;
    }

    template< class System >
    void do_step( System system )
    {
        std::cout << "dense out stepper: " << m_t << " , " << m_dt << std::endl;
        m_t += m_dt;
    }

    void calc_state( const time_type t_inter , state_type &x )
    {
        x = m_x;
    }

    const state_type& current_state() const
    { return m_x; }

    time_type current_time() const
    { return m_t; }

    time_type current_time_step() const
    { return m_dt; }


private :
    time_type m_t;
    time_type m_dt;
    state_type m_x;
};


void lorenz( const state_type &x , state_type &dxdt , const value_type t )
{
    //const value_type sigma( 10.0 );
    const value_type R( 28.0 );
    const value_type b( value_type( 8.0 ) / value_type( 3.0 ) );

    // first component trivial
    dxdt[0] = 1.0; //sigma * ( x[1] - x[0] );
    dxdt[1] = R * x[0] - x[1] - x[0] * x[2];
    dxdt[2] = -b * x[2] + x[0] * x[1];
}

struct push_back_time
{
    std::vector< double >& m_times;

    state_type& m_x;

    push_back_time( std::vector< double > &times , state_type &x )
    :  m_times( times ) , m_x( x ) { }

    void operator()( const state_type &x , double t )
    {
        m_times.push_back( t );
        boost::numeric::odeint::copy( x , m_x );
    }
};

template< class Stepper >
struct perform_integrate_const_test
{
    void operator()()
    {
        state_type x( 3 , 10.0 ) , x_end( 3 );

        std::vector< value_type > times;

        Stepper stepper;

        integrate_const( boost::ref(stepper) , lorenz , x , 0.0 , 1.0 ,
                         0.1, push_back_time( times , x_end ) );
    }
};

template< class Stepper >
struct perform_integrate_adaptive_test
{
    void operator()()
    {
        state_type x( 3 , 10.0 ) , x_end( 3 );

        std::vector< value_type > times;

        Stepper stepper;

        integrate_adaptive( boost::ref(stepper) , lorenz , x , 0.0 , 1.0 ,
                            0.1, push_back_time( times , x_end ) );
    }
};

template< class Stepper >
struct perform_integrate_n_steps_test
{
    void operator()()
    {
        state_type x( 3 , 10.0 ) , x_end( 3 );

        std::vector< value_type > times;

        Stepper stepper;

        integrate_n_steps( boost::ref(stepper) , lorenz , x , 0.0 , 0.1 ,
                           10 , push_back_time( times , x_end ) );
    }
};

template< class Stepper >
struct perform_integrate_times_test
{
    void operator()()
    {
        state_type x( 3 , 10.0 ) , x_end( 3 );

        std::vector< value_type > times;

        Stepper stepper;

        integrate_times( boost::ref(stepper) , lorenz , x ,
                         boost::counting_iterator<int>(0) , boost::counting_iterator<int>(10) , 0.1 ,
                         push_back_time( times , x_end ) );
    }
};

class stepper_methods : public mpl::vector<
    simple_stepper_nc< state_type > ,
    controlled_stepper_nc< state_type >,
    dense_out_stepper_nc< state_type > > { };

BOOST_AUTO_TEST_SUITE( integrate_stepper_refs )

BOOST_AUTO_TEST_CASE_TEMPLATE( integrate_const_test_case , Stepper, stepper_methods )
{
    std::cout << "integrate const" << std::endl;
    perform_integrate_const_test< Stepper > tester;
    tester();
}


BOOST_AUTO_TEST_CASE_TEMPLATE( integrate_adaptive_test_case , Stepper, stepper_methods )
{
    std::cout << "integrate adaptive" << std::endl;
    perform_integrate_adaptive_test< Stepper > tester;
    tester();
}

BOOST_AUTO_TEST_CASE_TEMPLATE( integrate_n_steps_test_case , Stepper, stepper_methods )
{
    std::cout << "integrate n steps" << std::endl;
    perform_integrate_n_steps_test< Stepper > tester;
    tester();
}

BOOST_AUTO_TEST_CASE_TEMPLATE( integrate_times_test_case , Stepper, stepper_methods )
{
    std::cout << "integrate times" << std::endl;
    perform_integrate_times_test< Stepper > tester;
    tester();
}

BOOST_AUTO_TEST_SUITE_END()
