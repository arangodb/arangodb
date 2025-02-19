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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>

#include "RestServer/arangod.h"

struct TRI_vocbase_t;  // forward declaration

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief a flexible way to get at the system vocbase
///        can be used for persisting configuration
////////////////////////////////////////////////////////////////////////////////
class SystemDatabaseFeature final : public ArangodFeature {
 public:
  struct VocbaseReleaser {
    void operator()(TRI_vocbase_t* ptr);
  };

  using ptr = std::unique_ptr<TRI_vocbase_t, VocbaseReleaser>;

  static constexpr std::string_view name() noexcept { return "SystemDatabase"; }

  explicit SystemDatabaseFeature(Server& server,
                                 TRI_vocbase_t* vocbase = nullptr);

  void start() override;
  void unprepare() override;
  ptr use() const;

 private:
  // cached pointer to the system database
  std::atomic<TRI_vocbase_t*> _vocbase;
};

}  // namespace arangodb
