////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/AttributeNameParser.h"
#include "Basics/Result.h"
#include "Inspection/Types.h"

#include <velocypack/Slice.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace arangodb::vector {

// Distance metric shared by the IVF vector index and the vector-graph index.
enum class Metric : std::uint8_t {
  kL2,
  kCosine,
  kInnerProduct,
};

template<class Inspector>
inline auto inspect(Inspector& f, Metric& x) {
  return f.enumeration(x).values(Metric::kL2, "l2", Metric::kCosine, "cosine",
                                 Metric::kInnerProduct, "innerProduct");
}

// Reads the vector stored under the index's first attribute path from `doc` and
// appends its components to `output`. Fails if the field is absent, is not an
// array, does not match `dimension`, or holds a non-numeric element. Shared by
// the IVF vector index and the vector-graph index.
Result readDocumentVectorData(
    velocypack::Slice doc,
    std::vector<std::vector<basics::AttributeName>> const& fields,
    std::size_t dimension, std::vector<float>& output);

}  // namespace arangodb::vector
