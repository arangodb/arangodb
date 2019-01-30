////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_SELECTIVITY_ESTIMATES_H
#define ARANGOD_CLUSTER_SELECTIVITY_ESTIMATES_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

namespace arangodb {
class LogicalCollection;

class ClusterSelectivityEstimates {
 public:
  typedef std::unordered_map<std::string, double> ValueType;

  ClusterSelectivityEstimates(ClusterSelectivityEstimates const&) = delete;
  ClusterSelectivityEstimates(ClusterSelectivityEstimates&&) = delete;
  ClusterSelectivityEstimates& operator=(ClusterSelectivityEstimates const&);
  ClusterSelectivityEstimates& operator=(ClusterSelectivityEstimates&&);

  explicit ClusterSelectivityEstimates(LogicalCollection*);
  ~ClusterSelectivityEstimates();

  ValueType fetch();
  ValueType get() const;

  bool defined() const;

  static constexpr double defaultExpireTime = 15.0;

 private:
  mutable basics::ReadWriteLock _lock;

  LogicalCollection* _collection;
  ValueType _estimates;
  double _expires;
  bool _updating;
};

}  // namespace arangodb

#endif
