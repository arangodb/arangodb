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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <faiss/invlists/InvertedLists.h>
#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>

#include "RocksDBIndex.h"
#include "Indexes/VectorIndexDefinition.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

namespace arangodb {
class LogicalCollection;
class RocksDBMethods;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class RocksDBVectorIndex;

using Quantitizer =
    std::variant<faiss::IndexFlat, faiss::IndexFlatL2, faiss::IndexFlatIP>;

class RocksDBVectorIndex final : public RocksDBIndex {
 public:
  RocksDBVectorIndex(IndexId iid, LogicalCollection& coll,
                     arangodb::velocypack::Slice info);

  IndexType type() const override { return Index::TRI_IDX_TYPE_VECTOR_INDEX; }

  bool isSorted() const override { return false; }

  bool canBeDropped() const override { return true; }

  // TODO
  bool hasSelectivityEstimate() const override { return false; }

  char const* typeName() const override { return "rocksdb-vector"; }

  bool matchesDefinition(VPackSlice const&) const override;

  void prepareIndex(std::unique_ptr<rocksdb::Iterator> it, rocksdb::Slice upper,
                    RocksDBMethods* methods) override;

  void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;
  FullVectorIndexDefinition const& getDefinition() const noexcept {
    return _definition;
  }

  std::pair<std::vector<LocalDocumentId::BaseType>, std::vector<float>>
  readBatch(std::vector<float>& inputs, RocksDBMethods* rocksDBMethods,
            transaction::Methods* trx,
            std::shared_ptr<LogicalCollection> collection, std::size_t count,
            std::size_t topK);

  UserVectorIndexDefinition const& getVectorIndexDefinition() override;

 protected:
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& /*options*/) override;

 private:
  void finishTraining();

  FullVectorIndexDefinition _definition;
  Quantitizer _quantizer;
  std::size_t _trainingDataSize;
};

}  // namespace arangodb
