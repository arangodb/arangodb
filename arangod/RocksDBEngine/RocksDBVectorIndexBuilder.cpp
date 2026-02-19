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

#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "RocksDBEngine/RocksDBBuilderIndex.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <thread>

#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBVectorIndexList.h"
#include "Transaction/Helpers.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include "faiss/IndexFlat.h"
#include "faiss/IndexIVFFlat.h"
#include "faiss/impl/io.h"
#include "faiss/index_factory.h"
#include "faiss/index_io.h"
#include "faiss/utils/distances.h"

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
                          transaction::helpers::extractKeyFromDocument(doc)
                              .copyString()
                              .c_str())};
    }

    if (!value.isArray()) {
      return {TRI_ERROR_TYPE_ERROR,
              std::format("array expected for vector attribute for document {}",
                          transaction::helpers::extractKeyFromDocument(doc)
                              .copyString()
                              .c_str())};
    }

    if (value.length() != dimension) {
      return {TRI_ERROR_TYPE_ERROR,
              std::format(
                  "provided vector is not of matching dimension for document "
                  "{}, index dimension: {}, document dimension: {}",
                  transaction::helpers::extractKeyFromDocument(doc)
                      .copyString()
                      .c_str(),
                  dimension, value.length())};
    }

    // We don't make assumptions here if output contains one or more vectors
    for (auto const d : VPackArrayIterator(value)) {
      if (not d.isNumber<double>()) {
        return {
            TRI_ERROR_TYPE_ERROR,
            std::format("vector contains data not representable as double for "
                        "document {}",
                        transaction::helpers::extractKeyFromDocument(doc)
                            .copyString()
                            .c_str())};
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

TrainingResult VectorIndexTrainer::restoreFromTrainedData(
    TrainedData const& data) {
  faiss::VectorIOReader reader;
  // TODO prevent this copy, but instead implement own IOReader, reading
  // directly from the training data.
  reader.data = data.codeData;
  auto faissIndex = std::unique_ptr<faiss::IndexIVF>{
      dynamic_cast<faiss::IndexIVF*>(faiss::read_index(&reader))};
  ADB_PROD_ASSERT(faissIndex != nullptr);

  TrainingResult result;
  result.faissIndex = std::move(faissIndex);
  // trainedData is not set — caller already has it.
  return result;
}

VectorIndexTrainer::VectorIndexTrainer(
    UserVectorIndexDefinition const& definition, bool isSparse,
    std::vector<std::vector<basics::AttributeName>> const& fields,
    std::string_view shardName, std::uint64_t indexId)
    : _definition(definition),
      _isSparse(isSparse),
      _fields(fields),
      _shardName(shardName),
      _indexId(indexId) {}

std::shared_ptr<faiss::IndexIVF> VectorIndexTrainer::createFaissIndex() const {
  if (_definition.factory) {
    std::shared_ptr<faiss::Index> index(faiss::index_factory(
        _definition.dimension, _definition.factory->c_str(),
        metricToFaissMetric(_definition.metric)));

    auto ivfIndex = std::dynamic_pointer_cast<faiss::IndexIVF>(index);
    if (ivfIndex == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "Index definition not supported. Expected IVF index.");
    }

    return ivfIndex;
  } else {
    auto quantizer = std::invoke([&]() -> std::unique_ptr<faiss::Index> {
      switch (_definition.metric) {
        case SimilarityMetric::kL2:
          return std::make_unique<faiss::IndexFlatL2>(_definition.dimension);
        case SimilarityMetric::kCosine:
          return std::make_unique<faiss::IndexFlatIP>(_definition.dimension);
        case SimilarityMetric::kInnerProduct:
          return std::make_unique<faiss::IndexFlatIP>(_definition.dimension);
      }
    });

    std::shared_ptr<faiss::IndexIVF> ivfIndex =
        std::make_unique<faiss::IndexIVFFlat>(
            quantizer.get(), _definition.dimension, _definition.nLists,
            metricToFaissMetric(_definition.metric));
    ivfIndex->own_fields = nullptr != quantizer.release();
    return ivfIndex;
  }
}

std::vector<float> VectorIndexTrainer::collectTrainingDataset(
    rocksdb::Iterator& it, rocksdb::Slice upper,
    std::int64_t maxVectors) const {
  std::vector<float> trainingData;
  std::vector<float> input;
  input.reserve(_definition.dimension);
  std::int64_t counter{0};

  LOG_TOPIC("b161b", INFO, Logger::STATISTICS)
      << "[shard=" << _shardName << ", index=" << _indexId << "] "
      << "Loading " << maxVectors << " vectors of dimension "
      << _definition.dimension << " for training.";

  while (counter < maxVectors && it.Valid()) {
    TRI_ASSERT(it.key().compare(upper) < 0);
    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it.value().data()));
    if (auto const res =
            readDocumentVectorData(doc, _fields, _definition.dimension, input);
        res.fail()) {
      if (res.is(TRI_ERROR_BAD_PARAMETER) && _isSparse) {
        it.Next();
        continue;
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(),
                                     "invalid index type definition");
    }

    trainingData.insert(trainingData.end(), input.begin(), input.end());
    input.clear();

    it.Next();
    ++counter;
  }

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, counter, trainingData.data());
  }

  if (trainingData.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "For the vector index to be created documents "
        "must be present in the respective collection for the training "
        "process.");
  }

  return trainingData;
}

TrainingResult VectorIndexTrainer::train(rocksdb::Iterator& it,
                                         rocksdb::Slice upper) const {
  // Create the FAISS index with the resolved nLists.
  auto faissIndex = createFaissIndex();

  // Collect training vectors and train the FAISS index.
  std::int64_t trainingDataSize =
      faissIndex->cp.max_points_per_centroid * _definition.nLists;
  auto const trainingData = collectTrainingDataset(it, upper, trainingDataSize);
  auto numVectors =
      static_cast<std::int64_t>(trainingData.size() / _definition.dimension);

  LOG_TOPIC("a162b", INFO, Logger::STATISTICS)
      << "[shard=" << _shardName << ", index=" << _indexId << "] "
      << "Loaded " << numVectors << " vectors. Start training process on "
      << _definition.nLists << " centroids.";

  faissIndex->train(numVectors, trainingData.data());

  LOG_TOPIC("a160b", INFO, Logger::STATISTICS)
      << "[shard=" << _shardName << ", index=" << _indexId << "] "
      << "Finished training.";

  // Persist the resolved defaultNProbe into IndexIVF::nprobe so that it
  // survives serialization and can be read back on restore without needing
  // to recompute from the (no-longer-available) original document count.
  faissIndex->nprobe = _definition.defaultNProbe;

  // Serialize the trained FAISS index
  faiss::VectorIOWriter writer;
  faiss::write_index(faissIndex.get(), &writer);

  TrainingResult result;
  result.faissIndex = std::move(faissIndex);
  result.trainedData.codeData = std::move(writer.data);
  return result;
}

VectorIndexBuildManager::VectorIndexBuildManager(RocksDBVectorIndex& index)
    : _index(index),
      _engine(index.collection().vocbase().engine<RocksDBEngine>()),
      _rootDB(_engine.db()->GetRootDB()),
      _rcoll(
          static_cast<RocksDBCollection*>(index.collection().getPhysical())) {}

Result VectorIndexBuildManager::build() {
  auto& coll = _index.collection();
  auto const bounds = _rcoll->bounds();

  VectorIndexTrainer trainer(_index.getDefinition(), _index.sparse(),
                             _index.fields(), coll.name(), _index.id().id());

  rocksdb::Slice upper(bounds.end());
  rocksdb::ReadOptions ro(false, false);
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;

  auto* docCF = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);

  // Training may be triggered from an insert() call while the enclosing
  // transaction has not yet committed.  In that case the RocksDB iterator
  // won't see any documents.  Wait until at least one document is visible.
  static constexpr int kMaxWaitIterations = 300;
  static constexpr auto kWaitInterval = std::chrono::milliseconds(100);
  for (int i = 0; i < kMaxWaitIterations; ++i) {
    std::unique_ptr<rocksdb::Iterator> probe(_rootDB->NewIterator(ro, docCF));
    probe->Seek(bounds.start());
    if (probe->Valid()) {
      break;
    }
    std::this_thread::sleep_for(kWaitInterval);
  }

  std::unique_ptr<rocksdb::Iterator> trainIt(_rootDB->NewIterator(ro, docCF));
  trainIt->Seek(bounds.start());

  auto result = trainer.train(*trainIt, upper);

  LOG_TOPIC("e163b", INFO, Logger::ENGINES)
      << "[shard=" << coll.name() << ", index=" << _index.id().id() << "] "
      << "Training complete. Ingesting vectors via RocksDBBuilderIndex.";

  _index.applyTrainingResult(std::move(result));

  auto self = coll.lookupIndex(_index.id());
  if (!self) {
    return {TRI_ERROR_ARANGO_INDEX_NOT_FOUND, "index was dropped during build"};
  }
  auto selfRocksDB = std::static_pointer_cast<RocksDBIndex>(self);
  auto builder = std::make_shared<RocksDBBuilderIndex>(
      selfRocksDB, _rcoll->meta().numberDocuments(), /*parallelism*/ 2);

  RocksDBBuilderIndex::Locker locker(_rcoll);
  std::move(locker.lock()).waitAndGet();
  auto res = std::move(builder->fillIndexBackground(locker)).waitAndGet();

  if (res.fail()) {
    LOG_TOPIC("e164b", ERR, Logger::ENGINES)
        << "[shard=" << coll.name() << ", index=" << _index.id().id() << "] "
        << "Vector ingestion failed: " << res.errorMessage();
  } else {
    LOG_TOPIC("e165b", INFO, Logger::ENGINES)
        << "[shard=" << coll.name() << ", index=" << _index.id().id() << "] "
        << "Deferred training and ingestion completed.";
  }

  return res;
}

}  // namespace arangodb::vector
