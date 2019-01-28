//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef PERF_HPP
#define PERF_HPP

// this header contains general purpose functions and variables used by
// the boost.compute performance benchmarks.

#include <vector>
#include <cstdlib>
#include <algorithm>

#include <boost/lexical_cast.hpp>
#include <boost/timer/timer.hpp>

static size_t PERF_N = 1024;
static size_t PERF_TRIALS = 3;

// parses command line arguments and sets the corresponding perf variables
inline void perf_parse_args(int argc, char *argv[])
{
    if(argc >= 2){
        PERF_N = boost::lexical_cast<size_t>(argv[1]);
    }

    if(argc >= 3){
        PERF_TRIALS = boost::lexical_cast<size_t>(argv[2]);
    }
}

// generates a vector of random numbers
template<class T>
std::vector<T> generate_random_vector(const size_t size)
{
    std::vector<T> vector(size);
    std::generate(vector.begin(), vector.end(), rand);
    return vector;
}

// a simple timer wrapper which records multiple time entries
class perf_timer
{
public:
    typedef boost::timer::nanosecond_type nanosecond_type;

    perf_timer()
    {
        timer.stop();
    }

    void start()
    {
        timer.start();
    }

    void stop()
    {
        timer.stop();
        times.push_back(timer.elapsed().wall);
    }

    size_t trials() const
    {
        return times.size();
    }

    void clear()
    {
        times.clear();
    }

    nanosecond_type last_time() const
    {
        return times.back();
    }

    nanosecond_type min_time() const
    {
        return *std::min_element(times.begin(), times.end());
    }

    nanosecond_type max_time() const
    {
        return *std::max_element(times.begin(), times.end());
    }

    boost::timer::cpu_timer timer;
    std::vector<boost::timer::nanosecond_type> times;
};

// returns the rate (in MB/s) for processing 'count' items of type 'T'
// in 'time' nanoseconds
template<class T>
double perf_rate(const size_t count, perf_timer::nanosecond_type time)
{
    const size_t byte_count = count * sizeof(T);

    return (double(byte_count) / 1024 / 1024) / (time / 1e9);
}

#endif // PERF_HPP
