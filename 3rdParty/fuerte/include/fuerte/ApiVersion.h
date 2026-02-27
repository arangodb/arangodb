////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace arangodb {
namespace fuerte {
inline namespace v1 {

/// @brief API version constants shared between fuerte and the ArangoDB server.
/// Defined here (in fuerte) so that fuerte does not need to include server
/// headers.  lib/Rest/ApiVersion.h re-exports these values.
struct ApiVersion {
  /// Version used when no /_arango/vX prefix is present
  static constexpr uint32_t defaultApiVersion = 0;

  /// Version accessed via /_arango/experimental
  static constexpr uint32_t experimentalApiVersion = 2;
};

}  // namespace v1
}  // namespace fuerte
}  // namespace arangodb
