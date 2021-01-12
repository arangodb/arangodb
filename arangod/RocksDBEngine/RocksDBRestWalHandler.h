////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_REST_WAL_HANDLER_H
#define ARANGOD_ROCKSDB_ROCKSDB_REST_WAL_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

class RocksDBRestWalHandler : public RestBaseHandler {
 public:
  RocksDBRestWalHandler(application_features::ApplicationServer&,
                        GeneralRequest*, GeneralResponse*);

 public:
  RequestLane lane() const override final {
    return RequestLane::SERVER_REPLICATION;
  }
  RestStatus execute() override final;
  char const* name() const override final { return "RocksDBRestWalHandler"; }

 private:
  void flush();
  void transactions();
  void properties();
};
}  // namespace arangodb

#endif
