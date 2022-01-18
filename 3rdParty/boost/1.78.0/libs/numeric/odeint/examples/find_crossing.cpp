/*
 * find_crossing.cpp
 *
 * Finds the energy threshold crossing for a damped oscillator.
 * The algorithm uses a dense out stepper with find_if to first find an
 * interval containing the threshold crossing and the utilizes the dense out
 * functionality with a bisection to further refine the interval until some
 * desired precision is reached.
 *
 * Copyright 2015 Mario Mulansky
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or
 * copy at http://www.boost.org/LICENSE_1_0.txt)
 */



#include <iostream>
#include <utility>
#include <algorithm>
#include <array>

#include <boost/numeric/odeint/stepper/runge_kutta_dopri5.hpp>
#include <boost/numeric/odeint/stepper/generation.hpp>
#include <boost/numeric/odeint/iterator/adaptive_iterator.hpp>

namespace odeint = boost::numeric::odeint;

typedef std::array<double, 2> state_type;

const double gam = 1.0;  // damping strength

void damped_osc(const state_type &x, state_type &dxdt, const double /*t*/)
{
    dxdt[0] = x[1];
    dxdt[1] = -x[0] - gam * x[1];
}


struct energy_condition {

    // defines the threshold crossing in terms of a boolean functor

    double m_min_energy;

    energy_condition(const double min_energy)
        : m_min_energy(min_energy) { }

    double energy(const state_type &x) {
        return 0.5 * x[1] * x[1] + 0.5 * x[0] * x[0];
    }

    bool operator()(const state_type &x) {
        // becomes true if the energy becomes smaller than the threshold
        return energy(x) <= m_min_energy;
    }
};


template<class System, class Condition>
std::pair<double, state_type>
find_condition(state_type &x0, System sys, Condition cond,
               const double t_start, const double t_end, const double dt,
               const double precision = 1E-6) {

    // integrates an ODE until some threshold is crossed
    // returns time and state at the point of the threshold crossing
    // if no threshold crossing is found, some time > t_end is returned

    auto stepper = odeint::make_dense_output(1.0e-6, 1.0e-6,
                                             odeint::runge_kutta_dopri5<state_type>());

    auto ode_range = odeint::make_adaptive_range(std::ref(stepper), sys, x0,
                                                 t_start, t_end, dt);

    // find the step where the condition changes
    auto found_iter = std::find_if(ode_range.first, ode_range.second, cond);

    if(found_iter == ode_range.second)
    {
        // no threshold crossing -> return time after t_end and ic
        return std::make_pair(t_end + dt, x0);
    }

    // the dense out stepper now covers the interval where the condition changes
    // improve the solution by bisection
    double t0 = stepper.previous_time();
    double t1 = stepper.current_time();
    double t_m;
    state_type x_m;
    // use odeint's resizing functionality to allocate memory for x_m
    odeint::adjust_size_by_resizeability(x_m, x0,
                                         typename odeint::is_resizeable<state_type>::type());
    while(std::abs(t1 - t0) > precision) {
        t_m = 0.5 * (t0 + t1);  // get the mid point time
        stepper.calc_state(t_m, x_m); // obtain the corresponding state
        if (cond(x_m))
            t1 = t_m;  // condition changer lies before midpoint
        else
            t0 = t_m;  // condition changer lies after midpoint
    }
    // we found the interval of size eps, take it's midpoint as final guess
    t_m = 0.5 * (t0 + t1);
    stepper.calc_state(t_m, x_m);
    return std::make_pair(t_m, x_m);
}


int main(int argc, char **argv)
{
    state_type x0 = {{10.0, 0.0}};
    const double t_start = 0.0;
    const double t_end = 10.0;
    const double dt = 0.1;
    const double threshold = 0.1;

    energy_condition cond(threshold);
    state_type x_cond;
    double t_cond;
    std::tie(t_cond, x_cond) = find_condition(x0, damped_osc, cond,
                                              t_start, t_end, dt, 1E-6);
    if(t_cond > t_end)
    {
        // time after t_end -> no threshold crossing within [t_start, t_end]
        std::cout << "No threshold crossing found." << std::endl;
    } else
    {
        std::cout.precision(16);
        std::cout << "Time of energy threshold crossing: " << t_cond << std::endl;
        std::cout << "State: [" << x_cond[0] << " , " << x_cond[1] << "]" << std::endl;
        std::cout << "Energy: " << cond.energy(x_cond) << std::endl;
    }
}
