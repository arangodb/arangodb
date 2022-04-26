////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>

#include "RocksDBOptions.h"
#include "Workloads/InsertDocuments.h"

namespace arangodb::sepp {

struct IndexSetup {
  std::string name;
  std::string type;
  std::vector<std::string> fields;
};

template<class Inspector>
auto inspect(Inspector& f, IndexSetup& o) {
  return f.object(o).fields(f.field("name", o.name).fallback(""),
                            f.field("type", o.type),
                            f.field("fields", o.fields));
}

struct CollectionsSetup {
  std::string name;
  std::string type;
  std::vector<IndexSetup> indexes;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionsSetup& o) {
  return f.object(o).fields(f.field("name", o.name),
                            f.field("type", o.type).fallback("document"),
                            f.field("indexes", o.indexes).fallback(f.keep()));
}

struct Setup {
  std::vector<CollectionsSetup> collections;
};

template<class Inspector>
auto inspect(Inspector& f, Setup& o) {
  return f.object(o).fields(f.field("collections", o.collections));
}

struct Options {
  std::string databaseDirectory;
  std::uint32_t runtime;
  std::uint32_t rounds;

  Setup setup;
  workloads::InsertDocuments::Options workload;

  RocksDBOptions rocksdb;
};

template<class Inspector>
auto inspect(Inspector& f, Options& o) {
  return f.object(o).fields(
      f.field("databaseDirectory", o.databaseDirectory).fallback("/tmp/sepp"),
      f.field("runtime", o.runtime).fallback(10000u),      //
      f.field("setup", o.setup),                           //
      f.field("workload", o.workload).fallback(f.keep()),  //
      f.field("rounds", o.rounds).fallback(5u), f.field("rocksdb", o.rocksdb));
}

}  // namespace arangodb::sepp