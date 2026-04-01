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
#include "Basics/ResultT.h"
#include "VectorIndex/VectorIndexDefinition.h"
#include "Metrics/Fwd.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"

#include <cstdint>
#include <memory>
#include <stop_token>
#include <string_view>
#include <vector>

#include <faiss/IndexIVF.h>
#include <rocksdb/iterator.h>
#include <rocksdb/slice.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class DB;
}  // namespace rocksdb

namespace arangodb {
class Index;
class RocksDBIndex;
class RocksDBVectorIndex;
class RocksDBEngine;
}  // namespace arangodb

namespace arangodb::vector {

TrainedData serializeIndex(faiss::IndexIVF const& index);

Result readDocumentVectorData(
    velocypack::Slice doc,
    std::vector<std::vector<basics::AttributeName>> const& fields,
    std::size_t dimension, std::vector<float>& output);

class VectorIndexTrainer {
 public:
  VectorIndexTrainer(
      UserVectorIndexDefinition const& definition, bool isSparse,
      std::vector<std::vector<basics::AttributeName>> const& fields,
      std::string_view shardName, std::uint64_t indexId,
      std::int64_t trainingThreshold);

  static std::shared_ptr<faiss::IndexIVF> restoreFromTrainedData(
      TrainedData const& data);

  std::shared_ptr<faiss::IndexIVF> createFaissIndex(
      std::int64_t resolvedNLists) const;

  /// Run the full training pipeline:
  ///  1. Resolve nLists from the definition
  ///  2. Create the FAISS index
  ///  3. Load training vectors from the iterator
  ///  4. Train the FAISS index
  ///  5. Serialize the trained index data
  ResultT<std::shared_ptr<faiss::IndexIVF>> train(
      rocksdb::Iterator& it, rocksdb::Slice upper, std::uint64_t numDocsHint,
      std::stop_token stopToken = {}) const;

 private:
  ResultT<std::vector<float>> collectTrainingDataset(
      rocksdb::Iterator& it, rocksdb::Slice upper, std::int64_t maxVectors,
      std::stop_token stopToken) const;

  /// Resolve the nLists value from the definition, using numDocsHint for
  /// scaling mode.
  std::int64_t resolveNLists(std::uint64_t numDocsHint) const;

  UserVectorIndexDefinition const& _definition;
  bool _isSparse;
  std::vector<std::vector<basics::AttributeName>> const& _fields;
  std::string _shardName;
  std::uint64_t _indexId;
  std::int64_t _trainingThreshold;
};

Result ingestVectors(RocksDBVectorIndex& index, rocksdb::DB* rootDB,
                     std::unique_ptr<rocksdb::Iterator> documentIterator,
                     std::stop_token stopToken = {});

class VectorIndexBuilder {
 public:
  explicit VectorIndexBuilder(RocksDBVectorIndex& index);

  Result build(std::shared_ptr<RocksDBIndex> indexPtr,
               metrics::Histogram<metrics::LogScale<double>>& trainingDuration,
               metrics::Histogram<metrics::LogScale<double>>& ingestionDuration,
               std::stop_token stopToken = {});

 private:
  RocksDBVectorIndex& _index;
  RocksDBEngine& _engine;
  rocksdb::DB* _rootDB;
  RocksDBCollection* _rcoll;
  RocksDBKeyBounds const _bounds;
};

}  // namespace arangodb::vector
