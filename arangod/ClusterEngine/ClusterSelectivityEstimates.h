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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <utility>
#include <mutex>

#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class LogicalCollection;
class TransactionId;

///@brief basic cache for selectivity estimates in the cluster
class ClusterSelectivityEstimates {
 public:
  explicit ClusterSelectivityEstimates(LogicalCollection& collection);
  void flush();

  /// @brief fetch estimates from cache or server
  /// @param allowUpdate allow cluster communication
  /// @param tid specify ongoing transaction this is a part of
  IndexEstMap get(bool allowUpdate, TransactionId tid);
  void set(IndexEstMap estimates);

 private:
  struct InternalData {
    IndexEstMap estimates;
    double expireStamp;

    InternalData(IndexEstMap estimates, double expireStamp)
        : estimates(std::move(estimates)), expireStamp(expireStamp) {}
  };

  LogicalCollection& _collection;
  // the current estimates, only load and stored using atomic operations
  std::shared_ptr<InternalData> _data;
  // whether or not a thread is currently updating the estimates
  std::mutex _update;

  static constexpr double defaultTtl = 180.0;
  static constexpr double systemCollectionTtl = 900.0;
};

}  // namespace arangodb
