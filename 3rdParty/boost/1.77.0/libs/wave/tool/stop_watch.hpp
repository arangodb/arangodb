/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2001-2012 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_STOP_WATCH_HPP_HK040911_INCLUDED)
#define BOOST_STOP_WATCH_HPP_HK040911_INCLUDED

#include <boost/config.hpp>
#include <boost/timer/timer.hpp>

///////////////////////////////////////////////////////////////////////////////
//  
class stop_watch : public boost::timer::cpu_timer {

public:
    
    std::string format_elapsed_time() const
    {
        boost::timer::cpu_times times = elapsed();
        double current = static_cast<double>(times.user + times.system) / 1.e9;

        char time_buffer[sizeof("1234:56:78.90 abcd.") + 1];

        using namespace std;
        if (current >= 3600) {
            // show hours
            sprintf (time_buffer, "%d:%02d:%02d.%03d hrs.",
                (int)(current) / 3600, ((int)(current) % 3600) / 60,
                ((int)(current) % 3600) % 60, 
                (int)(current * 1000) % 1000);
        }
        else if (current >= 60) {
            // show minutes
            sprintf (time_buffer, "%d:%02d.%03d min.", 
                (int)(current) / 60, (int)(current) % 60, 
                (int)(current * 1000) % 1000);
        }
        else {
            // show seconds
            sprintf(time_buffer, "%d.%03d sec.", (int)current, 
                (int)(current * 1000) % 1000);
        }
        return time_buffer;
    }
};

#endif // !defined(BOOST_STOP_WATCH_HPP_HK040911_INCLUDED)
