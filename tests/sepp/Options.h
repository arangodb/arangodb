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
#include <variant>

#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/Thread.h"
#include "RocksDBOptions.h"

#include "Inspection/Types.h"
#include "Inspection/VPackLoadInspector.h"
#include "Workloads/GetByPrimaryKey.h"
#include "Workloads/InsertDocuments.h"
#include "Workloads/IterateDocuments.h"

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
  std::map<std::string, workloads::InsertDocuments::Options> prefill;
};

template<class Inspector>
auto inspect(Inspector& f, Setup& o) {
  return f.object(o).fields(f.field("collections", o.collections),
                            f.field("prefill", o.prefill).fallback(f.keep()));
}

using WorkloadVariants = std::variant<workloads::GetByPrimaryKey::Options,
                                      workloads::InsertDocuments::Options,
                                      workloads::IterateDocuments::Options>;
namespace workloads {
// this inspect function must be in namespace workloads for ADL to pick it up
template<class Inspector>
inline auto inspect(Inspector& f, WorkloadVariants& o) {
  namespace insp = arangodb::inspection;
  return f.variant(o).unqualified().alternatives(
      insp::type<workloads::GetByPrimaryKey::Options>("getByPrimaryKey"),
      insp::type<workloads::InsertDocuments::Options>("insert"),
      insp::type<workloads::IterateDocuments::Options>("iterate"));
}
}  // namespace workloads

struct Options {
  std::string databaseDirectory;
  bool clearDatabaseDirectory{true};

  Setup setup;

  WorkloadVariants workload;

  RocksDBOptions rocksdb;
};

template<class Inspector>
auto inspect(Inspector& f, Options& o) {
  return f.object(o).fields(
      f.field("databaseDirectory", o.databaseDirectory)
          .fallback(basics::FileUtils::buildFilename(
              TRI_GetTempPath(),
              "sepp-" + std::to_string(Thread::currentProcessId()))),
      f.field("clearDatabaseDirectory", o.clearDatabaseDirectory)
          .fallback(true),
      f.field("setup", o.setup).fallback(f.keep()),        //
      f.field("workload", o.workload).fallback(f.keep()),  //
      f.field("rocksdb", o.rocksdb).fallback(f.keep()));
}

}  // namespace arangodb::sepp
