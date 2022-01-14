#include <boost/config.hpp>
#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE odeint_controlled_adams_bashforth_moulton

#include <boost/test/unit_test.hpp>

#include <boost/numeric/odeint/stepper/adaptive_adams_bashforth_moulton.hpp>
#include <boost/numeric/odeint/stepper/controlled_adams_bashforth_moulton.hpp>

using namespace boost::unit_test;
using namespace boost::numeric::odeint;

struct const_sys
{
    template< class State , class Deriv , class Value >
    void operator()( const State &x , Deriv &dxdt , const Value &dt ) const
    {
        dxdt[0] = 1;
    }
};

typedef boost::array< double , 1 > state_type;
typedef double value_type;

BOOST_AUTO_TEST_SUITE( controlled_adams_bashforth_moulton_test )

BOOST_AUTO_TEST_CASE( test_instantiation )
{
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<1, state_type> > s1;
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<2, state_type> > s2;
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<3, state_type> > s3;
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<4, state_type> > s4;
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<5, state_type> > s5;
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<6, state_type> > s6;
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<7, state_type> > s7;
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<8, state_type> > s8;
    controlled_adams_bashforth_moulton<adaptive_adams_bashforth_moulton<9, state_type> > s9;

    state_type x = {{ 10.0 }};
    value_type t = 0.0 , dt = 0.01;

    s1.try_step(const_sys(), x, t, dt);
    s2.try_step(const_sys(), x, t, dt);
    s3.try_step(const_sys(), x, t, dt);
    s4.try_step(const_sys(), x, t, dt);
    s5.try_step(const_sys(), x, t, dt);
    s6.try_step(const_sys(), x, t, dt);
    s7.try_step(const_sys(), x, t, dt);
    s8.try_step(const_sys(), x, t, dt);
    s9.try_step(const_sys(), x, t, dt);
}

BOOST_AUTO_TEST_SUITE_END()