////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <vector>
#include <span>
#include <cstddef>
#include <iosfwd>

namespace arangodb {
namespace velocypack {
class Slice;
}
class LocalDocumentId;

struct IndexDistinctScanOptions {
  // positions of the fields used to find distinct tuples
  std::vector<std::size_t> distinctFields;
  // whether the returned distinct tuples should be sorted
  bool sorted;
};

std::ostream& operator<<(std::ostream&, IndexDistinctScanOptions const&);

struct AqlIndexDistinctScanIterator {
  using SliceType = velocypack::Slice;
  virtual ~AqlIndexDistinctScanIterator() = default;
  virtual bool next(std::span<SliceType> values) = 0;
};

}  // namespace arangodb
