////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_UTILS_H
#define ARANGODB_PREGEL_UTILS_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace pregel {
  class Utils {
    Utils() = delete;
    
  public:
    // constants
    static std::string const nextGSSPath;
    static std::string const finishedGSSPath;
    static std::string const messagesPath;
    
    static std::string const executionNumberKey;
    static std::string const vertexCollectionKey;
    static std::string const edgeCollectionKey;
    static std::string const algorithmKey;
    static std::string const coordinatorIdKey;
    
    static std::string const globalSuperstepKey;
    static std::string const messagesKey;
  
  };
}
}
#endif
