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
#include "lib/Basics/StringUtils.h"
#include "Logger/Logger.h"

#include <iostream>
#include <regex>
#include <vector>

bool arangodb::basics::parse_dateTime(
    std::string const& dateTimeIn,
    std::chrono::system_clock::time_point& date_tp) {
    using namespace date;
    using namespace std::chrono;

    std::string dateTime = dateTimeIn;
    std::string strDate, strTime;

    boost::algorithm::trim(dateTime);

    std::regex iso8601_regex("(\\+|\\-)?\\d+(\\-\\d{1,2}(\\-\\d{1,2})?)?(((\\ |T)\\d\\d\\:\\d\\d(\\:\\d\\d(\\.\\d{1,3})?)?(z|Z|(\\+|\\-)\\d\\d\\:\\d\\d)?)?|(z|Z)?)?");

    if (!std::regex_match(dateTime, iso8601_regex)) {
        LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "regex failed for datetime '" << dateTime << "'";
        return false;
    }

    strDate = dateTime;

    if (strDate.back() == 'Z' || strDate.back() == 'z') {
        strDate.pop_back();
    }

    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "parse datetime '" << strDate << "'";

    if (strDate.find("T") != std::string::npos || strDate.find(" ") != std::string::npos) { // split into ymd / time
        std::vector<std::string> strs;
        boost::split(strs, strDate, boost::is_any_of("T "));

        strDate = strs[0];
        strTime = strs[1];
     } // if

    // parse date component
    int parsedYear, parsedMonth = 1, parsedDay = 1, numParsedComponents;

    numParsedComponents = sscanf(strDate.c_str(), "%d-%d-%d", &parsedYear, &parsedMonth, &parsedDay);

    if (numParsedComponents == 0 || numParsedComponents == EOF) {
        LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "sscanf parse failed with " << numParsedComponents;
        return false;
    }

    if (1 < numParsedComponents &&
      (parsedMonth < 1 || 12 < parsedMonth)) {
            return false;
    }

    if (2 < numParsedComponents &&
       (parsedDay < 1 || 31 < parsedDay)) {
        return false;
    }

    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "parsed YMD " <<
    parsedYear << " " << parsedMonth << " " << parsedDay;

    date_tp = sys_days(year{parsedYear}/parsedMonth/parsedDay);

     // parse Time HH:MM:SS(.SSS)((+|-)HH:MM)
     if (1 < strTime.size()) {
        /* REGEX GROUPS
        12:34:56.789-12:34
        submatch 0: '12:34:56.789-12:34'
        submatch 1: '12'
        submatch 2: '34'
        submatch 3: ':56.789'
        submatch 4: '56'
        submatch 5: '.789'
        submatch 6: '789'
        submatch 7: '-12:34'
        submatch 8: '-'
        submatch 9: '12'
        submatch 10: '34'
        */

        std::regex time_regex("(\\d\\d)\\:(\\d\\d)(\\:(\\d\\d)(\\.(\\d{1,3}))?)?((\\+|\\-)(\\d\\d)\\:(\\d\\d))?");
        std::smatch time_parts;
         
        if (!std::regex_match(strTime, time_parts, time_regex)) {
            LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "regex failed for time " << strTime;
            return false;
        }

         // hour
         hours parsedHours{atoi(time_parts[1].str().c_str())};

         if (hours{23} < parsedHours) {
             return false;
         }
         date_tp += parsedHours;

         // minute
         minutes parsedMinutes{atoi(time_parts[2].str().c_str())};

         if (minutes{59} < parsedMinutes) {
             return false;
         }
         date_tp += parsedMinutes;

         // seconds
         seconds parsedSeconds{atoi(time_parts[4].str().c_str())};

         if (seconds{59} < parsedSeconds) {
             return false;
         }
         date_tp += parsedSeconds;

         // milliseconds .9 -> 900ms
         date_tp += milliseconds{atoi( (time_parts[6].str() + "00").substr(0,3).c_str() )};

         // time offset
         if (time_parts[8].str().size()) {
             hours parsedHours{atoi(time_parts[9].str().c_str())}; // hours

             if (hours{23} < parsedHours) {
                 return false;
             }
             minutes offset = parsedHours;

             minutes parsedMinutes{atoi(time_parts[10].str().c_str())}; // minutes

            if (minutes{59} < parsedMinutes) {
                return false;
            }
            offset += parsedMinutes;

            if (time_parts[8].str() == "-") {
                offset *= -1;
            }
            date_tp -= offset;
         }
    } // if

    return true;
}
