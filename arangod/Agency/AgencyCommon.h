////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef __ARANGODB_CONSENSUS_AGENCY_COMMON__
#define __ARANGODB_CONSENSUS_AGENCY_COMMON__

#include <vector>
#include <string>

namespace arangodb {
namespace consensus {
  
  /**
   * @brief Agent configuration
   */
  template<class T> struct Config {
    T min_ping;
    T max_ping;
    std::vector<std::string> end_points;
    Config () : min_ping(.15), max_ping(.3) {};
    Config (T min_p, T max_p, std::vector<std::string>& end_p) :
      min_ping(min_p), max_ping(max_p), end_points(end_p) {}
  }; 
  
}}
#endif // __ARANGODB_CONSENSUS_AGENT__

