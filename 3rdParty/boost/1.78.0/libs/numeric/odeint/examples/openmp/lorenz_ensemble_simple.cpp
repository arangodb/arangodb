/* Boost libs/numeric/odeint/examples/openmp/lorenz_ensemble_simple.cpp

 Copyright 2013 Karsten Ahnert
 Copyright 2013 Mario Mulansky
 Copyright 2013 Pascal Germroth

 Parallelized Lorenz ensembles

 Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <omp.h>
#include <vector>
#include <iostream>
#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/external/openmp/openmp.hpp>
#include "point_type.hpp"

using namespace std;

typedef vector<double> vector_type;
typedef point<double, 3> point_type;
typedef vector<point_type> state_type;

const double sigma = 10.0;
const double b = 8.0 / 3.0;

struct sys_func {
    const vector_type &R;
    sys_func( const vector_type &_R ) : R( _R ) { }

    void operator()( const state_type &x , state_type &dxdt , double t ) const {
        const size_t n = x.size();
#       pragma omp parallel for
        for(size_t i = 0 ; i < n ; i++) {
            const point_type &xi = x[i];
            point_type &dxdti = dxdt[i];
            dxdti[0] = -sigma * (xi[0] - xi[1]);
            dxdti[1] = R[i] * xi[0] - xi[1] - xi[0] * xi[2];
            dxdti[2] = -b * xi[2] + xi[0] * xi[1];
        }
    }
};


int main() {
    using namespace boost::numeric::odeint;

    const size_t n = 1024;
    vector_type R(n);
    const double Rmin = 0.1, Rmax = 50.0;
#   pragma omp parallel for
    for(size_t i = 0 ; i < n ; i++)
        R[i] = Rmin + (Rmax - Rmin) / (n - 1) * i;

    state_type X(n, point_type(10, 10, 10));

    const double t_max = 10.0, dt = 0.01;

    // Simple stepper with constant step size
    // typedef runge_kutta4<state_type, double, state_type, double,
    //                      openmp_range_algebra> stepper;

    // integrate_const(stepper(), sys_func(R), X, 0.0, t_max, dt);

    // Controlled stepper with variable step size
    typedef runge_kutta_fehlberg78<state_type, double, state_type, double,
                                   openmp_range_algebra> error_stepper_type;
    typedef controlled_runge_kutta<error_stepper_type> controlled_stepper_type;
    controlled_stepper_type controlled_stepper;

    integrate_adaptive(controlled_stepper, sys_func(R), X, 0.0, t_max, dt);

    copy( X.begin(), X.end(), ostream_iterator<point_type>(cout, "\n") );

    return 0;
}
