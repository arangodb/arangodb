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

namespace arangodb::sepp {

struct Options {
  std::string databaseDirectory;
  std::size_t runtime;
  std::size_t rounds;

  RocksDBOptions rocksdb;
};

template<class Inspector>
auto inspect(Inspector& f, Options& o) {
  return f.object(o).fields(
      f.field("databaseDirectory", o.databaseDirectory).fallback("/tmp/sepp"),
      f.field("runtime", o.runtime).fallback(10000u),
      f.field("rounds", o.rounds).fallback(5u), f.field("rocksdb", o.rocksdb));
}

}  // namespace arangodb::sepp