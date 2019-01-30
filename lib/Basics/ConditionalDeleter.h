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

#ifndef ARANGODB_BASICS_CONDITIONAL_DELETER_H
#define ARANGODB_BASICS_CONDITIONAL_DELETER_H 1

#include "Basics/Common.h"

namespace arangodb {

/// @brief a custom deleter that deletes an object only if the condition is true
/// to be used in conjunction with unique or shared ptrs when ownership needs to
/// be transferred
template <typename T>
struct ConditionalDeleter {
  explicit ConditionalDeleter(bool& condition) : condition(condition) {}
  void operator()(T* object) {
    if (condition) {
      delete object;
    }
  }

  bool& condition;
};

}  // namespace arangodb

#endif
