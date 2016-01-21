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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_OPERATION_MODE_H
#define ARANGOD_REST_OPERATION_MODE_H 1

#include "Basics/Common.h"

#include "Basics/ProgramOptions.h"

namespace arangodb {
namespace rest {



////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server operation modes
////////////////////////////////////////////////////////////////////////////////

class OperationMode {
 public:
  typedef enum {
    MODE_CONSOLE,
    MODE_UNITTESTS,
    MODE_SCRIPT,
    MODE_SERVER
  } server_operation_mode_e;

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the server operation mode
  //////////////////////////////////////////////////////////////////////////////

  static server_operation_mode_e determineMode(
      const arangodb::basics::ProgramOptions& options) {
    if (options.has("console")) {
      return MODE_CONSOLE;
    } else if (options.has("javascript.unit-tests")) {
      return MODE_UNITTESTS;
    } else if (options.has("javascript.script")) {
      return MODE_SCRIPT;
    } else {
      return MODE_SERVER;
    }
  }
};
}
}

#endif


