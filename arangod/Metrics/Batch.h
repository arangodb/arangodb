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

#include "Basics/debugging.h"
#include "Containers/NodeHashMap.h"
#include "Metrics/IBatch.h"
#include "Metrics/Metric.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <vector>

namespace arangodb::metrics {

template<typename T>
class Batch final : public IBatch {
 public:
  void toPrometheus(std::string& result, std::string_view globals,
                    bool ensureWhitespace) const final {
    TRI_ASSERT(!_metrics.empty());
    std::vector<typename T::Data> metrics;
    metrics.reserve(_metrics.size());
    for (auto& [_, metric] : _metrics) {
      metrics.push_back(metric.load());  // synchronization here
    }
    // note: Serialization works only for counter, gauge, untyped metrics
    // It doesn't work for histogram, but its possible, just
    // remove addMark call and make kToString more powerful
    for (size_t i = 0; i != T::kSize; ++i) {
      Metric::addInfo(result, T::kName[i], T::kHelp[i], T::kType[i]);
      for (size_t j = 0; auto& [labels, _] : _metrics) {
        Metric::addMark(result, T::kName[i], globals, labels);
        absl::StrAppend(&result, ensureWhitespace ? " " : "",
                        T::kToString[i](metrics[j++]), "\n");
      }
    }
  }

  void toVPack(velocypack::Builder& builder, ClusterInfo& ci) const final {
    for (auto& [labels, metric] : _metrics) {
      if (T::skip(ci, labels)) {
        continue;
      }
      auto const coordinatorLabels = T::coordinatorLabels(labels);
      auto const data = metric.load();
      // TODO(MBkkt) Write labels once
      for (size_t i = 0; i != T::kSize; ++i) {
        builder.add(velocypack::Value{T::kName[i]});
        builder.add(velocypack::Value{coordinatorLabels});
        builder.add(velocypack::Value{T::kToValue[i](data)});
      }
    }
  }

  T& add(std::string_view labels) {
    return _metrics[labels];  // for breakpoint
  }
  size_t remove(std::string_view labels) final {
    auto const size = _metrics.size();
    return size - _metrics.erase(labels);
  }

 private:
  containers::NodeHashMap<std::string, T> _metrics;
};

}  // namespace arangodb::metrics
