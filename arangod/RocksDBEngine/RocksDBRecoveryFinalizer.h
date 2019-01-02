////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ROCKSDB_RECOVERY_FINALIZER_H
#define ROCKSDB_RECOVERY_FINALIZER_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
// a small glue feature that only establishes the dependencies between the RocksDBEngine
// feature and the DatabaseFeature
// its start method will be run after both the RocksDBEngine and the DatabaseFeature
// have started and all databases have been established
// it will then call the DatabaseFeature's recoveryDone() method, which will start
// replication in all databases if nececessary
class RocksDBRecoveryFinalizer final : public application_features::ApplicationFeature {
 public:
  explicit RocksDBRecoveryFinalizer(application_features::ApplicationServer* server);

 public:
  void start() override final;
};
}  // namespace arangodb

#endif
