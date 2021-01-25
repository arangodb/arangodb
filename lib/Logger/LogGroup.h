////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_LOGGER_LOG_GROUP_H
#define ARANGODB_LOGGER_LOG_GROUP_H 1

#include <cstddef>

namespace arangodb {

class LogGroup {
 public:
  // @brief Number of log groups; must increase this when a new group is added
  static constexpr std::size_t Count = 2;
  virtual ~LogGroup() = default;

  /// @brief Must return a UNIQUE identifier amongst all LogGroup derivatives
  /// and must be less than Count
  virtual std::size_t id() const = 0;
};

}  // namespace arangodb

#endif
