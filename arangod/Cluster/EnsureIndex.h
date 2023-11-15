////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ActionBase.h"
#include "ActionDescription.h"
#include "VocBase/Methods/Indexes.h"

#include <chrono>

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;

namespace maintenance {

class EnsureIndex : public ActionBase {
 public:
  EnsureIndex(MaintenanceFeature&, ActionDescription const& d);

  virtual ~EnsureIndex();

  virtual arangodb::Result setProgress(double d) override final;
  virtual bool first() override final;

  static void indexCreationLogging(VPackSlice index);

 private:
  static auto ensureIndexReplication2(
      std::shared_ptr<LogicalCollection> coll, VPackSlice indexInfo,
      std::shared_ptr<methods::Indexes::ProgressTracker> progress) noexcept
      -> Result;
};

}  // namespace maintenance
}  // namespace arangodb
