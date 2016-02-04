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

#include <cstdint>
#include <velocypack/vpack.h>
#include "AgencyCommon.h"

//using namespace arangodb::velocypack;

class Slice {};

namespace arangodb {
namespace consensus {

/**
 * @brief Log repilca
 */
class Log {
  
public:
  typedef uint64_t index_t;
  enum ret_t {OK, REDIRECT};

  /**
   * @brief Default constructor
   */
  Log ();

  /**
   * @brief Default Destructor
   */
  virtual ~Log();

  /**
   * @brief Log
   */

  ret_t log (std::shared_ptr<arangodb::velocypack::Builder> const);
  
private:
  
  index_t _commit_id;      /**< @brief: index of highest log entry known
                              to be committed (initialized to 0, increases monotonically) */
  index_t _last_applied;   /**< @brief: index of highest log entry applied to state machine  */
};
  
}}
