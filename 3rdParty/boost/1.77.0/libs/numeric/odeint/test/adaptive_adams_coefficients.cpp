#include <boost/config.hpp>
#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE odeint_adaptive_adams_coefficients

#include <boost/test/unit_test.hpp>

#include <boost/numeric/odeint/stepper/detail/adaptive_adams_coefficients.hpp>

#include <vector>

#include <boost/mpl/list.hpp>
#include <boost/mpl/size_t.hpp>
#include <boost/mpl/range_c.hpp>

using namespace boost::unit_test;
using namespace boost::numeric::odeint;

typedef double value_type;

BOOST_AUTO_TEST_SUITE( adaptive_adams_coefficients_test )

typedef boost::mpl::range_c< size_t , 2 , 10 > vector_of_steps;
BOOST_AUTO_TEST_CASE_TEMPLATE( test_step, step_type, vector_of_steps )
{
    const static size_t steps = step_type::value;

    typedef std::vector<double> deriv_type;
    typedef double time_type;

    typedef detail::adaptive_adams_coefficients<steps, deriv_type, time_type> aac_type;

    std::vector<double> deriv;
    deriv.push_back(-1);

    time_type t = 0.0;
    time_type dt = 0.1;

    aac_type coeff;
    for(size_t i=0; i<steps; ++i)
    {
        coeff.predict(t, dt);
        coeff.do_step(deriv);
        coeff.confirm();

        t+= dt;

        if(coeff.m_eo < steps)
            coeff.m_eo ++;
    }

    std::vector<value_type> v(10);
    v[0] = 1.0/1.0;
    v[1] = 1.0/2.0;
    v[2] = 5.0/12.0;
    v[3] = 9.0/24.0;
    v[4] = 251.0/720.0;
    v[5] = 95.0/288.0;
    v[6] = 19087.0/60480.0;
    v[7] = 5257.0/17280.0;
    v[8] = 5311869667636789.0/18014398509481984.0;

    for(size_t i=0; i<steps; ++i)
    {
        BOOST_CHECK_SMALL(coeff.beta[1][i] - 1.0, 1e-15);

        if(i == 0)
            BOOST_CHECK_SMALL(coeff.phi[2][i].m_v[0] + 1, 1e-15);
        else if (i == steps-1 && steps%2 == 1)
            BOOST_CHECK_SMALL(coeff.phi[2][i].m_v[0] - 1, 1e-14);
        else if (i == steps-1 && steps%2 == 0)
            BOOST_CHECK_SMALL(coeff.phi[2][i].m_v[0] + 1, 1e-14);
        else
            BOOST_CHECK_SMALL(coeff.phi[2][i].m_v[0], 1e-15);

        BOOST_CHECK_SMALL(coeff.g[i] - v[i], 1e-15);
    }
}

BOOST_AUTO_TEST_CASE( test_copy )
{
    typedef std::vector<double> deriv_type;
    typedef double time_type;

    typedef detail::adaptive_adams_coefficients<3, deriv_type, time_type> aac_type;
    aac_type c1;

    deriv_type deriv(1);
    deriv[0] = 1.0;

    time_type t = 0.0;
    time_type dt = 0.01;

    for(size_t i=0; i<3; ++i)
    {
        c1.predict(t, dt);
        c1.do_step(deriv);
        c1.confirm();

        t+= dt;

        if(c1.m_eo < 3)
            c1.m_eo ++;
    }

    aac_type c2(c1);
    BOOST_CHECK_EQUAL(c1.phi[0][0].m_v[0], c2.phi[0][0].m_v[0]);
    BOOST_CHECK(&(c1.phi[0][0].m_v) != &(c2.phi[0][0].m_v));

    aac_type c3;
    deriv_type *p1 = &(c3.phi[0][0].m_v);

    c3 = c1;
    BOOST_CHECK(p1 == (&(c3.phi[0][0].m_v)));
    BOOST_CHECK_EQUAL(c1.phi[0][0].m_v[0], c3.phi[0][0].m_v[0]);
}

BOOST_AUTO_TEST_SUITE_END()