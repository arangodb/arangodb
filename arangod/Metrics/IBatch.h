////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "RestServer/arangod.h"

#include <string>
#include <string_view>

namespace arangodb {
class ClusterInfo;
}
namespace arangodb::velocypack {

class Builder;

}  // namespace arangodb::velocypack
namespace arangodb::metrics {

class IBatch {
 public:
  virtual void toPrometheus(std::string& result, std::string_view globals,
                            bool ensureWhitespace) const = 0;

  virtual void toVPack(velocypack::Builder& builder, ClusterInfo&) const = 0;

  virtual size_t remove(std::string_view labels) = 0;

  virtual ~IBatch();
};

}  // namespace arangodb::metrics
