////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Basics/Guarded.h"
#include "Containers/FlatHashMap.h"

namespace arangodb {
namespace velocypack {
class Slice;
}

class Result;

struct CurrentWatcher {
  void reserve(size_t expectedSize);

  void addWatchPath(std::string path, std::string identifier,
                    std::function<bool(velocypack::Slice)> callback);

  bool hasReported(std::string const& identifier) const;
  void addReport(std::string identifier, Result result);
  bool haveAllReported() const;
  std::vector<std::tuple<std::string, std::string,
                         std::function<bool(velocypack::Slice)>>> const&
  getCallbackInfos() const;

  std::optional<Result> getResultIfAllReported() const;
  void clearCallbacks();

 private:
  // Tuple has:
  // path, identifier, callback
  std::vector<std::tuple<std::string, std::string,
                         std::function<bool(velocypack::Slice)>>>
      _callbacks{};
  Guarded<absl::flat_hash_map<std::string, Result>> _results;
  size_t _expectedResults{0};
};

}  // namespace arangodb
