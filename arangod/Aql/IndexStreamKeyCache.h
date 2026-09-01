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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <algorithm>
#include <span>
#include <vector>

#include <velocypack/Slice.h>
#include <velocypack/String.h>

namespace arangodb::aql {

template<typename SliceType>
struct IndexStreamKeyCache {
  void update(std::span<SliceType const> src, std::span<SliceType> dst) {
    std::copy(src.begin(), src.end(), dst.begin());
  }
};

template<>
struct IndexStreamKeyCache<velocypack::Slice> {
  void update(std::span<velocypack::Slice const> src,
              std::span<velocypack::Slice> dst) {
    _owned.resize(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
      _owned[i] = src[i];
      dst[i] = _owned[i].slice();
    }
  }

 private:
  std::vector<velocypack::String> _owned;
};

}  // namespace arangodb::aql
