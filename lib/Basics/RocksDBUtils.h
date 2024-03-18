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
/// @author Daniel H. Larkin
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"

#include <rocksdb/status.h>
#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <memory>
#include <utility>

namespace arangodb::rocksutils {

enum StatusHint { none, document, collection, view, index, database, wal };

arangodb::Result convertStatus(rocksdb::Status const&,
                               StatusHint hint = StatusHint::none);

std::pair<VPackSlice, std::unique_ptr<VPackBuffer<uint8_t>>> stripObjectIds(
    VPackSlice inputSlice, bool checkBeforeCopy = true);

}  // namespace arangodb::rocksutils
