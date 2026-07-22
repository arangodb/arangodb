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

#include "VectorIndex/VectorIndexUtils.h"

#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Inspection/Format.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "Transaction/Helpers.h"

#include <velocypack/Iterator.h>

#include <format>

namespace arangodb::vector {

Result readDocumentVectorData(
    velocypack::Slice doc,
    std::vector<std::vector<basics::AttributeName>> const& fields,
    std::size_t dimension, std::vector<float>& output) {
  TRI_ASSERT(fields.size() >= 1);

  try {
    VPackSlice value = rocksutils::accessDocumentPath(doc, fields[0]);

    // this fails if index is not sparse
    if (value.isNone()) {
      return {TRI_ERROR_BAD_PARAMETER,
              std::format("vector field not present in document {}",
                          transaction::helpers::extractKeyFromDocument(doc))};
    }

    if (!value.isArray()) {
      return {TRI_ERROR_TYPE_ERROR,
              std::format("array expected for vector attribute for document {}",
                          transaction::helpers::extractKeyFromDocument(doc))};
    }

    if (value.length() != dimension) {
      return {TRI_ERROR_TYPE_ERROR,
              std::format("provided vector is not of matching dimension for "
                          "document {}, index dimension: {}, document "
                          "dimension: {}",
                          transaction::helpers::extractKeyFromDocument(doc),
                          dimension, value.length())};
    }

    // We don't make assumptions here if output contains one or more vectors
    for (auto const d : VPackArrayIterator(value)) {
      if (not d.isNumber<double>()) {
        return {
            TRI_ERROR_TYPE_ERROR,
            std::format("vector contains data not representable as double for "
                        "document {}",
                        transaction::helpers::extractKeyFromDocument(doc))};
      }
      output.push_back(d.getNumericValue<double>());
    }

    return {};
  } catch (velocypack::Exception const& e) {
    return {TRI_ERROR_TYPE_ERROR,
            std::format("deserialization error when accessing a document: {}",
                        e.what())};
  }
}

}  // namespace arangodb::vector
