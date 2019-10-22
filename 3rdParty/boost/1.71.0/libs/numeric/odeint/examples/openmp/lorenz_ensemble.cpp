/* Boost libs/numeric/odeint/examples/openmp/lorenz_ensemble.cpp

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
#include <iterator>
#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/external/openmp/openmp.hpp>
#include <boost/lexical_cast.hpp>
#include "point_type.hpp"

using namespace std;
using namespace boost::numeric::odeint;

typedef point<double, 3> point_type;
typedef vector< point_type > inner_state_type;
typedef openmp_state<point_type> state_type;

const double sigma = 10.0;
const double b = 8.0 / 3.0;


struct sys_func {
    const vector<double> &R;
    sys_func( vector<double> &R ) : R(R) {}

    void operator()( const state_type &x , state_type &dxdt , double t ) const {
#       pragma omp parallel for
        for(size_t j = 0 ; j < x.size() ; j++) {
            size_t offset = 0;
            for(size_t i = 0 ; i < j ; i++)
                offset += x[i].size();

            for(size_t i = 0 ; i < x[j].size() ; i++) {
                const point_type &xi = x[j][i];
                point_type &dxdti = dxdt[j][i];
                dxdti[0] = -sigma * (xi[0] - xi[1]);
                dxdti[1] = R[offset + i] * xi[0] - xi[1] - xi[0] * xi[2];
                dxdti[2] = -b * xi[2] + xi[0] * xi[1];
            }
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

    vector<point_type> inner(n, point_type(10, 10, 10));
    state_type state;
    split(inner, state);

    cerr << "openmp_state split " << n << " into";
    for(size_t i = 0 ; i != state.size() ; i++)
        cerr << ' ' << state[i].size();
    cerr << endl;

    typedef runge_kutta4< state_type, double > stepper;

    const double t_max = 10.0, dt = 0.01;

    integrate_const(
        stepper(),
        sys_func(R),
        state,
        0.0, t_max, dt
    );

    unsplit(state, inner);
    std::copy( inner.begin(), inner.end(), ostream_iterator<point_type>(cout, "\n") );

    return 0;
}
