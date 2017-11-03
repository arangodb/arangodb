////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_SCOPED_TIMER__H
#define ARANGODB_BASICS_SCOPED_TIMER__H 1


// c++11 only
#pragma once
#ifndef OBI_UTIL_SCOPED_TIMER_HPP
#define OBI_UTIL_SCOPED_TIMER_HPP

#include <utility>
#include <vector>
#include <chrono>
#include <functional>
#include <iostream>
#include <iomanip>
namespace arangodb {

namespace obi{ namespace util {

class scoped_timer {
public:  // defines
    using hclock    = std::chrono::high_resolution_clock;
    using hrcStrVec = std::vector<std::pair<hclock::time_point,std::string>>;
    using intStrVec = std::vector<std::pair<int,std::string>>;

private:  // variables
    hrcStrVec m_timePoints;
    std::function<intStrVec(intStrVec)> m_callback;
    bool m_enabled;
    bool m_add_dtor_entry;

public:  // functions
    scoped_timer(std::function<intStrVec(intStrVec)> callback = &scoped_timer::toSting)
        : m_callback(callback)
        , m_enabled(true)
        , m_add_dtor_entry(true)
    {
        m_timePoints.reserve(10);  //if you want time more than 10 - add template param?
        m_timePoints.emplace_back(hclock::time_point(),"");
        m_timePoints.back().first=hclock::now(); //defeats the purpose of emplace a bit :/
    }

    scoped_timer(std::string name) : m_callback(&scoped_timer::toSting)
                                   , m_enabled(true)
                                   , m_add_dtor_entry(true)
    {
        m_timePoints.reserve(10);
        m_timePoints.emplace_back(hclock::time_point(), name);
        m_timePoints.back().first=hclock::now();
    }

    ~scoped_timer(void) {
        if(m_add_dtor_entry) {
            m_timePoints.emplace_back(hclock::now(),"dtor");
        }
        if(m_enabled) {
            m_callback(calculate());
        }
    }

    void add_step(std::string&& str = "") {
        m_timePoints.emplace_back(hclock::now(),str);
    }

    void disable_dtor_entry() {
        m_add_dtor_entry = false;
    }

    intStrVec show() {
        return m_callback(calculate());
    }
    void disable() {
        m_enabled=false;
    }

    intStrVec show_and_disable() {
        disable();
        return show();
    }

    void set_name(std::string name) {
        m_timePoints[0].second = name;
    }

private:  // functions
    static long int
    get_time_diff(const hclock::time_point& t0, const hclock::time_point& t1) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
    }

    intStrVec calculate() {
        long int total_time = get_time_diff(m_timePoints.front().first
                                           ,m_timePoints.back().first
                                           );
        intStrVec times;
        times.emplace_back(std::make_pair(total_time
                                         ,m_timePoints.front().second
                                         )
                          );
        if (m_timePoints.size() > 2) {
            for (std::size_t i = 1; i < m_timePoints.size(); i++) {
                times.emplace_back(std::make_pair(get_time_diff(m_timePoints[i-1].first
                                                               ,m_timePoints[i].first)
                                                 ,m_timePoints[i].second
                                                 )
                                  );
            }
        }
        return times;
    }  // function - calculate

    static intStrVec toSting(intStrVec times) {
        int width = 15;
        std::cout << "\ntotal   : "
                  << std::setw(width) << times[0].first << " ns - "
                  << std::setprecision(8) << std::fixed << times[0].first / 1000.0 << " μs - "
                  << std::setprecision(8) << std::fixed << times[0].first / 1000000.0 << " ms - "
                  << std::setprecision(8) << std::fixed << times[0].first / 1000000000.0 << " s";

        if (!times[0].second.empty()) {
            std::cout << " - " << times[0].second;
        }
        std::cout << std::endl;

        if (times.size() > 1) {
            for (std::size_t i = 1; i < times.size(); i++) {
                std::cout << "step "  << std::setw(3) << i << ": "
                          << std::setw(width) << times[i].first << " ns"
                          << std::setprecision(1) << std::fixed
                          << " (" << std::setw(5) << 100*static_cast<float>(times[i].first)/static_cast<float>(times[0].first) << "%)";
                if (!times[i].second.empty()) {
                    std::cout << " - " << times[i].second;
                }
                std::cout << std::endl;
            }
        }
        return times;
    }  // function - to_string
} ;

}}  // namespace obi::util
#endif // OBI_UTIL_SCOPED_TIMER_HPP

}

#endif
