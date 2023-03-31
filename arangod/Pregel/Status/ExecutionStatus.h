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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>
#include <string>
#include <chrono>

#include <Inspection/VPackWithErrorT.h>
#include <CrashHandler/CrashHandler.h>
#include <Assertions/ProdAssert.h>
#include <velocypack/Builder.h>

namespace arangodb::pregel {

using TimePoint = std::chrono::steady_clock::time_point;
using OptTimePoint = std::optional<TimePoint>;
using Seconds = std::chrono::duration<double>;

struct Duration {
  OptTimePoint _start;
  OptTimePoint _finish;

  [[nodiscard]] bool hasStarted() const { return _start.has_value(); }
  [[nodiscard]] bool hasFinished() const { return _finish.has_value(); }

  void start() {
    ADB_PROD_ASSERT(not _start.has_value());
    _start.emplace(std::chrono::steady_clock::now());
  }
  void finish() {
    ADB_PROD_ASSERT(_start.has_value());
    ADB_PROD_ASSERT(not _finish.has_value());
    _finish.emplace(std::chrono::steady_clock::now());
  }
  [[nodiscard]] auto elapsedSeconds() const -> Seconds {
    ADB_PROD_ASSERT(_start.has_value());
    if (not _finish.has_value()) {
      return std::chrono::steady_clock::now() - *_start;
    } else {
      return *_finish - *_start;
    }
  }
};

struct ExecutionTimings {
  Duration loading;
  Duration computation;
  Duration storing;
  // FIXME: just sum the times above?
  Duration total;

  std::vector<Duration> gss;
};
template<typename Inspector>
auto inspect(Inspector& f, ExecutionTimings& x) {
  if constexpr (Inspector::isLoading) {
    return inspection::Status::Success{};
  } else {
    VPackBuilder b;
    {
      VPackObjectBuilder ob(&b);
      if (x.total.hasStarted()) {
        b.add("totalRuntime", x.total.elapsedSeconds().count());
      }
      if (x.loading.hasStarted()) {
        b.add("startupTime", x.loading.elapsedSeconds().count());
      }
      if (x.computation.hasStarted()) {
        b.add("computationTime", x.computation.elapsedSeconds().count());
      }
      if (x.storing.hasStarted()) {
        b.add("storageTime", x.storing.elapsedSeconds().count());
      }
      b.add(VPackValue("gssTimes"));
      {
        VPackArrayBuilder array(&b);
        for (auto const& gssTime : x.gss) {
          b.add(VPackValue(gssTime.elapsedSeconds().count()));
        }
      }
    }
    return f.apply(b.slice());
  }
}

}  // namespace arangodb::pregel
