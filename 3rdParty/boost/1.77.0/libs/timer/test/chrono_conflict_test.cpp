// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// Check that using Chrono and Timer in the same program does
//   not cause link errors.


#include <boost/chrono.hpp>
#include <boost/timer/timer.hpp>

int main()
{
    boost::chrono::steady_clock::now();
    boost::timer::cpu_timer cpt;
}
