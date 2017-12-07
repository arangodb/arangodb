////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include "Basics/datetime.h"

#include <iostream>
#include <vector>

bool arangodb::basics::parse_dateTime(std::string const& dateTime, std::chrono::system_clock::time_point& date_tp) {
//    std::cout << std::endl;
    using namespace date;
    using namespace std::chrono;

    std::istringstream in;
    milliseconds ms = milliseconds{0};
    minutes min = minutes{0};
    year_month_day ymd;
    year_month ym;
    year y;

    std::string strDate = dateTime;
    std::string strTime = "";
    std::string strOffset = "";
    std::string leftover = "";
    
    bool failed;
    
    if (strDate.back() == 'Z') {
        strDate.pop_back();
    }

    if (strDate.find("T") != std::string::npos) { // split into ymd / time
        std::vector<std::string> strs;
        boost::split(strs, strDate, boost::is_any_of("T"));
        
        strDate = strs[0];
        strTime = strs[1];
     } // if
     
     // std::cout << "PARSE DATE: " << strDate << std::endl;
     
     // parse Date YYYY-MM-DD
     in = std::istringstream{strDate};
     in >> parse("%Y", y);
     failed = in.fail();
     leftover.clear();
     getline(in, leftover);
     
     if (failed || leftover.size()) {
        in = std::istringstream{strDate};
        in >> parse("%Y-%m", ym);
        failed = in.fail();     
        leftover.clear();
        getline(in, leftover);
        
        if (failed || leftover.size()) {        
            in = std::istringstream{strDate};
            in >> parse("%Y-%m-%d", ymd);
            failed = in.fail();
            leftover.clear();
            getline(in, leftover);
            
            if (failed || leftover.size()) {
                // std::cout << "failed to parse\n";
                return false;
            } else {
                date_tp = sys_days(ymd);
            }
        } else {
            date_tp = sys_days(ym/1);
        }
     } else {
         date_tp = sys_days(y/1/1);
     }
     // end parse date
     
     // parse Time HH:MM:SS(.SSS)(Z|((+|-)HH:MM)
     
     if (1 < strTime.size()) {         
         std::string addition = "";
         std::string abbrev;
         
         if (strTime.find("-") != std::string::npos || strTime.find("+") != std::string::npos) {
            addition = "%Ez";
         }

         // std::cout << "PARSE TIME: " << strTime << std::endl;
         in = std::istringstream{strTime};
         in >> parse("%H:%M"+addition, ms, abbrev, min);
         failed = in.fail();
         leftover.clear();
         getline(in, leftover);

         if (failed || leftover.size()) {
             in = std::istringstream{strTime};
             in >> parse("%H:%M:%S"+addition, ms, abbrev, min);
             failed = in.fail();
             leftover.clear();
             getline(in, leftover);
         }
         
         if (failed || leftover.size()) {
             std::cout << "CANT PARSE TIME"  << failed << " " << leftover.size() << " '" << leftover << "'" << std::endl;
             return false;
         }
         // std::cout << "parsed time " << ms << " " << min << std::endl;
         date_tp += ms - min;
    } // if
    return true;
}
