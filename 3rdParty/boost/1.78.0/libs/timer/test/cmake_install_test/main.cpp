// Copyright 2019 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/timer/timer.hpp>

int main()
{
    boost::timer::cpu_timer timer;
    timer.stop();
}
