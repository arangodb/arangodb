/* Boost libs/numeric/odeint/examples/openmp/lorenz_ensemble_nested.cpp

 Copyright 2013 Karsten Ahnert
 Copyright 2013 Pascal Germroth
 Copyright 2013 Mario Mulansky

 Parallelized Lorenz ensembles using nested omp algebra

 Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <omp.h>
#include <vector>
#include <iostream>
#include <iterator>
#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/external/openmp/openmp.hpp>
#include <boost/lexical_cast.hpp>
#include "point_type.hpp"

using namespace std;
using namespace boost::numeric::odeint;

typedef point<double, 3> point_type;
typedef vector< point_type > state_type;

const double sigma = 10.0;
const double b = 8.0 / 3.0;


struct sys_func {
    const vector<double> &R;
    sys_func( vector<double> &R ) : R(R) {}

    void operator()( const state_type &x , state_type &dxdt , double t ) const {
#       pragma omp parallel for
        for(size_t i = 0 ; i < x.size() ; i++) {
            dxdt[i][0] = -sigma * (x[i][0] - x[i][1]);
            dxdt[i][1] = R[i] * x[i][0] - x[i][1] - x[i][0] * x[i][2];
            dxdt[i][2] = -b * x[i][2] + x[i][0] * x[i][1];
        }
    }
};


int main(int argc, char **argv) {
    size_t n = 1024;
    if(argc > 1) n = boost::lexical_cast<size_t>(argv[1]);

    vector<double> R(n);
    const double Rmin = 0.1, Rmax = 50.0;
#   pragma omp parallel for
    for(size_t i = 0 ; i < n ; i++)
        R[i] = Rmin + (Rmax - Rmin) / (n - 1) * i;

    state_type state( n , point_type(10, 10, 10) );

    typedef runge_kutta4< state_type, double , state_type , double ,
                          openmp_nested_algebra<vector_space_algebra> > stepper;

    const double t_max = 10.0, dt = 0.01;

    integrate_const(
        stepper(),
        sys_func(R),
        state,
        0.0, t_max, dt
    );

    std::copy( state.begin(), state.end(), ostream_iterator<point_type>(cout, "\n") );

    return 0;
}
