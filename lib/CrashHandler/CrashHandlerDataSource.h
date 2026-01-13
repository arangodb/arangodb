////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>
#include <vector>

#include <velocypack/SharedSlice.h>
#include "CrashHandlerRegistry.h"

namespace arangodb {

class CrashHandlerRegistry;

class CrashHandlerDataSource {
 public:
  CrashHandlerDataSource(CrashHandlerRegistry* crashHandlerInterface);

  virtual ~CrashHandlerDataSource();

  virtual velocypack::SharedSlice getCrashData() const = 0;

  virtual std::string_view getDataSourceName() const = 0;

 private:
  CrashHandlerRegistry* _crashHandlerInterface;
};

}  // namespace arangodb
