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
#include "Indexes/VectorIndexDefinition.h"

#include <faiss/IndexIVF.h>
#include <rocksdb/iterator.h>
#include <rocksdb/slice.h>
#include <velocypack/Slice.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace rocksdb {
class DB;
}  // namespace rocksdb

namespace arangodb {
class RocksDBVectorIndex;
}  // namespace arangodb

namespace arangodb::vector {

/// Result of the training or restoration process.
struct TrainingResult {
  std::shared_ptr<faiss::IndexIVF> faissIndex;
  /// Serialized FAISS index data.
  TrainedData trainedData;
};

/// Read vector data from a document given the field path and expected
/// dimension. Shared utility used by both training and index operations.
Result readDocumentVectorData(
    velocypack::Slice doc,
    std::vector<std::vector<basics::AttributeName>> const& fields,
    std::size_t dimension, std::vector<float>& output);

/// Encapsulates FAISS index creation and the training pipeline.
/// Holds the immutable configuration that is shared across operations,
/// keeping individual method signatures clean.
class VectorIndexTrainer {
 public:
  VectorIndexTrainer(
      UserVectorIndexDefinition const& definition, bool isSparse,
      std::vector<std::vector<basics::AttributeName>> const& fields,
      std::string_view shardName, std::uint64_t indexId);

  /// Restore a FAISS IndexIVF from previously serialized trained data.
  /// Returns the index along with resolved nLists and defaultNProbe values.
  /// trainedData in the result is std::nullopt (caller already has it).
  static TrainingResult restoreFromTrainedData(TrainedData const& data);

  /// Create a fresh (untrained) FAISS IndexIVF from the definition.
  /// If the definition has a factory string, uses faiss::index_factory.
  /// The factory string's {nLists} placeholder is resolved if present.
  /// Otherwise creates an IndexIVFFlat with the appropriate quantizer.
  std::shared_ptr<faiss::IndexIVF> createFaissIndex() const;

  /// Run the full training pipeline:
  ///  1. Resolve nLists and defaultNProbe from the definition
  ///  2. Create the FAISS index
  ///  3. Load training vectors from the iterator
  ///  4. Train the FAISS index
  ///  5. Serialize the trained index data
  TrainingResult train(rocksdb::Iterator& it, rocksdb::Slice upper) const;

 private:
  /// Collect training vectors from the iterator.
  /// Reads up to maxVectors documents, extracts vector data, applies cosine
  /// normalization if needed, and returns the flat training buffer.
  std::vector<float> collectTrainingDataset(rocksdb::Iterator& it,
                                            rocksdb::Slice upper,
                                            std::int64_t maxVectors) const;

  UserVectorIndexDefinition const& _definition;
  bool _isSparse;
  std::vector<std::vector<basics::AttributeName>> const& _fields;
  std::string_view _shardName;
  std::uint64_t _indexId;
};

/// Bulk-ingest all documents from the iterator into the trained vector index.
/// Uses a multi-threaded pipeline: reader -> encoder -> writer.
/// The faissIndex must already be trained.
Result ingestVectors(RocksDBVectorIndex& index,
                     std::shared_ptr<faiss::IndexIVF> const& faissIndex,
                     rocksdb::DB* rootDB,
                     std::unique_ptr<rocksdb::Iterator> documentIterator);

/// Full build pipeline for a vector index: train the FAISS index from
/// collection documents, apply the result, then bulk-ingest all vectors
/// using RocksDBBuilderIndex for ingestion + WAL catchup.
Result buildVectorIndex(RocksDBVectorIndex& index);

}  // namespace arangodb::vector
