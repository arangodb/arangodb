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
/// @author Tobias Gödderz
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/Executor/IResearchViewExecutorBase.tpp"
#include "IResearch/IResearchExecutionPool.h"
#include "IResearch/IResearchReadUtils.h"

namespace arangodb::aql {

template<typename ExecutionTraits>
IResearchViewExecutor<ExecutionTraits>::IResearchViewExecutor(Fetcher& fetcher,
                                                              Infos& infos)
    : Base{fetcher, infos}, _segmentOffset{0} {
  this->_storedValuesReaders.resize(
      this->_infos.getOutNonMaterializedViewRegs().size() *
      infos.parallelism());
  TRI_ASSERT(infos.heapSort().empty());
  TRI_ASSERT(infos.parallelism() > 0);
  _segmentReaders.resize(infos.parallelism());
}

template<typename ExecutionTraits>
IResearchViewExecutor<ExecutionTraits>::~IResearchViewExecutor() {
  if (_allocatedThreads || _demandedThreads) {
    this->_infos.parallelExecutionPool().releaseThreads(_allocatedThreads,
                                                        _demandedThreads);
  }
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::readPK(LocalDocumentId& documentId,
                                                    SegmentReader& reader) {
  TRI_ASSERT(!documentId.isSet());
  TRI_ASSERT(reader.itr);
  if (!reader.itr->next()) {
    return false;
  }
  ++reader.totalPos;
  ++reader.currentSegmentPos;
  if constexpr (Base::isMaterialized) {
    TRI_ASSERT(reader.doc);
    commonReadPK(reader.pkReader, reader.doc->value, documentId);
  }
  return true;
}

template<typename ExecutionTraits>
template<bool parallel>
bool IResearchViewExecutor<ExecutionTraits>::readSegment(
    SegmentReader& reader, std::atomic_size_t& bufferIdx) {
  bool gotData = false;
  while (reader.atMost) {
    if (!reader.itr) {
      if (!resetIterator(reader)) {
        reader.finalize();
        return gotData;
      }

      // segment is constant until the next resetIterator().
      // save it to don't have to look it up every time.
      if constexpr (Base::isMaterialized) {
        reader.segment = &this->_reader->segment(reader.readerOffset);
      }
    }

    if constexpr (Base::isMaterialized) {
      TRI_ASSERT(reader.pkReader.itr);
      TRI_ASSERT(reader.pkReader.value);
    }
    LocalDocumentId documentId;
    // try to read a document PK from iresearch
    bool const iteratorExhausted = !readPK(documentId, reader);

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next
      // reader.
      reader.finalize();
      return gotData;
    }
    if constexpr (Base::isMaterialized) {
      // The collection must stay the same for all documents in the buffer
      TRI_ASSERT(reader.segment ==
                 &this->_reader->segment(reader.readerOffset));
      if (!documentId.isSet()) {
        // No document read, we cannot write it.
        continue;
      }
    }
    auto current = [&] {
      if constexpr (parallel) {
        return bufferIdx.fetch_add(1);
      } else {
        return PushTag{};
      }
    }();
    auto& viewSegment = this->_reader->segment(reader.readerOffset);
    if constexpr (Base::isLateMaterialized) {
      this->_indexReadBuffer.makeValue(current, viewSegment, reader.doc->value);
    } else {
      this->_indexReadBuffer.makeValue(current, viewSegment, documentId);
    }
    --reader.atMost;
    gotData = true;

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
      this->_indexReadBuffer.makeSearchDoc(current, viewSegment,
                                           reader.doc->value);
    }

    // in the ordered case we have to write scores as well as a document
    if constexpr (ExecutionTraits::Ordered) {
      if constexpr (parallel) {
        (*reader.scr)(this->_indexReadBuffer.getScoreBuffer(
            current * this->infos().scoreRegistersCount()));
      } else {
        this->fillScores(*reader.scr);
      }
    }

    if constexpr (Base::usesStoredValues) {
      TRI_ASSERT(reader.doc);
      TRI_ASSERT(std::distance(_segmentReaders.data(), &reader) <
                 static_cast<ptrdiff_t>(_segmentReaders.size()));
      this->makeStoredValues(current, reader.doc->value,
                             std::distance(_segmentReaders.data(), &reader));
    }
    if constexpr (!parallel) {
      // doc and scores are both pushed, sizes must now be coherent
      this->_indexReadBuffer.assertSizeCoherence();
    }

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next reader.
      reader.finalize();

      // Here we have at least one document in _indexReadBuffer, so we may not
      // add documents from a new reader.
      return gotData;
    }
  }
  return gotData;
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);
  size_t const count = this->_reader->size();
  bool gotData = false;
  auto atMost = ctx.outputRow.numRowsLeft();
  TRI_ASSERT(this->_indexReadBuffer.empty());
  this->_isMaterialized = false;
  auto parallelism = std::min(count, this->_infos.parallelism());
  this->_indexReadBuffer.reset();
  std::atomic_size_t bufferIdx{0};
  // shortcut for sequential execution.
  if (parallelism == 1) {
    TRI_IF_FAILURE("IResearchFeature::failNonParallelQuery") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    this->_indexReadBuffer.preAllocateStoredValuesBuffer(
        atMost, this->_infos.getScoreRegisters().size(),
        this->_infos.getOutNonMaterializedViewRegs().size());
    auto& reader = _segmentReaders.front();
    while (!gotData && (_segmentOffset < count || reader.itr)) {
      if (!reader.itr) {
        reader.readerOffset = _segmentOffset++;
        TRI_ASSERT(reader.readerOffset < count);
      }
      reader.atMost = atMost;
      gotData = readSegment<false>(reader, bufferIdx);
    }
    return gotData;
  }
  auto const atMostInitial = atMost;
  // here parallelism can be used or not depending on the
  // current pipeline demand.
  auto const& clientCall = ctx.outputRow.getClientCall();
  auto limit = clientCall.getUnclampedLimit();
  bool const isUnlimited = limit == aql::AqlCall::Infinity{};
  TRI_ASSERT(isUnlimited || std::holds_alternative<std::size_t>(limit));
  if (!isUnlimited && atMostInitial != 0) {
    parallelism = std::clamp(std::get<std::size_t>(limit) / atMostInitial,
                             (size_t)1, parallelism);
  }
  // let's be greedy as it is more likely that we are
  // asked to read some "tail" documents to fill the block
  // and next time we would need all our parallelism again.
  auto& readersPool = this->infos().parallelExecutionPool();
  if (parallelism > (this->_allocatedThreads + 1)) {
    uint64_t deltaDemanded{0};
    if ((parallelism - 1) > _demandedThreads) {
      deltaDemanded = parallelism - 1 - _demandedThreads;
      _demandedThreads += deltaDemanded;
    }
    this->_allocatedThreads += readersPool.allocateThreads(
        static_cast<int>(parallelism - this->_allocatedThreads - 1),
        deltaDemanded);
    parallelism = this->_allocatedThreads + 1;
  }
  atMost = atMostInitial * parallelism;

  std::vector<futures::Future<bool>> results;
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  this->_indexReadBuffer.setForParallelAccess(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  results.reserve(parallelism - 1);
  // we must wait for our threads before bailing out
  // as we most likely will release segments and
  // "this" will also be invalid.
  auto cleanupThreads = irs::Finally([&]() noexcept {
    results.erase(std::remove_if(results.begin(), results.end(),
                                 [](auto& f) { return !f.valid(); }),
                  results.end());
    if (results.empty()) {
      return;
    }
    auto runners = futures::collectAll(results);
    runners.wait();
  });
  while (bufferIdx.load() < atMostInitial) {
    TRI_ASSERT(results.empty());
    size_t i = 0;
    size_t selfExecute{std::numeric_limits<size_t>::max()};
    auto toFetch = atMost - bufferIdx.load();
    while (toFetch && i < _segmentReaders.size()) {
      auto& reader = _segmentReaders[i];
      if (!reader.itr) {
        if (_segmentOffset >= count) {
          // no new segments. But maybe some existing readers still alive
          ++i;
          continue;
        }
        reader.readerOffset = _segmentOffset++;
      }
      reader.atMost = std::min(atMostInitial, toFetch);
      TRI_ASSERT(reader.atMost);
      toFetch -= reader.atMost;
      if (selfExecute < parallelism) {
        futures::Promise<bool> promise;
        auto future = promise.getFuture();
        if (ADB_UNLIKELY(!readersPool.run(
                [&, ctx = &reader, pr = std::move(promise)]() mutable {
                  try {
                    pr.setValue(readSegment<true>(*ctx, bufferIdx));
                  } catch (...) {
                    pr.setException(std::current_exception());
                  }
                }))) {
          TRI_ASSERT(false);
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              " Failed to schedule parallel search view reading");
        }
        // this should be noexcept as we've reserved above
        results.push_back(std::move(future));
      } else {
        selfExecute = i;
      }
      if (results.size() == (parallelism - 1)) {
        break;
      }
      ++i;
    }
    if (selfExecute < std::numeric_limits<size_t>::max()) {
      gotData |= readSegment<true>(_segmentReaders[selfExecute], bufferIdx);
    } else {
      TRI_ASSERT(results.empty());
      break;
    }
    // we run this in noexcept mode as with current implementation
    // we can not recover and properly wait for finish in case
    // of exception in the middle of collectAll or wait.
    [&]() noexcept {
      auto runners = futures::collectAll(results);
      runners.wait();
      for (auto& r : runners.result().get()) {
        gotData |= r.get();
      }
    }();
    results.clear();
  }
  // shrink to actual size so we can emit rows as usual
  this->_indexReadBuffer.setForParallelAccess(
      bufferIdx, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  return gotData;
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::resetIterator(
    SegmentReader& reader) {
  TRI_ASSERT(this->_filter);
  TRI_ASSERT(!reader.itr);

  auto& segmentReader = (*this->_reader)[reader.readerOffset];

  if constexpr (Base::isMaterialized) {
    auto it = iresearch::pkColumn(segmentReader);

    if (ADB_UNLIKELY(!it)) {
      LOG_TOPIC("bd01b", WARN, iresearch::TOPIC)
          << "encountered a sub-reader without a primary key column while "
             "executing a query, ignoring";
      return false;
    }

    resetColumn(reader.pkReader, std::move(it));
  }

  if constexpr (Base::usesStoredValues) {
    if (ADB_UNLIKELY(!this->getStoredValuesReaders(
            segmentReader, std::distance(_segmentReaders.data(), &reader)))) {
      return false;
    }
  }

  reader.itr = this->_filter->execute({
      .segment = segmentReader,
      .scorers = this->_scorers,
      .ctx = &this->_filterCtx,
      .wand = {},
  });
  TRI_ASSERT(reader.itr);
  reader.doc = irs::get<irs::document>(*reader.itr);
  TRI_ASSERT(reader.doc);

  if constexpr (ExecutionTraits::Ordered) {
    reader.scr = irs::get<irs::score>(*reader.itr);

    if (!reader.scr) {
      reader.scr = &irs::score::kNoScore;
      reader.numScores = 0;
    } else {
      reader.numScores = this->infos().scorers().size();
    }
  }

  reader.itr = segmentReader.mask(std::move(reader.itr));
  TRI_ASSERT(reader.itr);
  reader.currentSegmentPos = 0;
  return true;
}

template<typename ExecutionTraits>
void IResearchViewExecutor<ExecutionTraits>::reset(
    [[maybe_unused]] bool needFullCount) {
  Base::reset();

  // reset iterator state
  for (auto& r : _segmentReaders) {
    r.finalize();
    r.readerOffset = 0;
    r.totalPos = 0;
  }
  _segmentOffset = 0;
}

template<typename ExecutionTraits>
size_t IResearchViewExecutor<ExecutionTraits>::skip(size_t limit,
                                                    IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter);

  size_t const toSkip = limit;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  for (auto& r : _segmentReaders) {
    TRI_ASSERT(r.currentSegmentPos == 0);
    TRI_ASSERT(r.totalPos == 0);
    TRI_ASSERT(r.itr == nullptr);
  }
#endif
  auto& reader = _segmentReaders.front();
  for (size_t count = this->_reader->size(); _segmentOffset < count;) {
    reader.readerOffset = _segmentOffset++;
    if (!resetIterator(reader)) {
      continue;
    }

    while (limit && reader.itr->next()) {
      ++reader.currentSegmentPos;
      ++reader.totalPos;
      --limit;
    }

    if (!limit) {
      break;  // do not change iterator if already reached limit
    }
    reader.finalize();
  }
  if constexpr (Base::isMaterialized) {
    reader.segment = &this->_reader->segment(reader.readerOffset);
    this->_isMaterialized = false;
    this->_indexReadBuffer.reset();
  }
  return toSkip - limit;
}

template<typename ExecutionTraits>
size_t IResearchViewExecutor<ExecutionTraits>::skipAll(IResearchViewStats&) {
  TRI_ASSERT(this->_filter);
  size_t skipped = 0;

  auto reset = [](SegmentReader& reader) {
    reader.itr.reset();
    reader.doc = nullptr;
    reader.currentSegmentPos = 0;
  };

  auto const count = this->_reader->size();
  if (_segmentOffset > count) {
    return skipped;
  }
  irs::Finally seal = [&]() noexcept {
    _segmentOffset = count + 1;
    this->_indexReadBuffer.clear();
  };
  if (this->infos().filterConditionIsEmpty()) {
    skipped = this->_reader->live_docs_count();
    size_t totalPos = std::accumulate(
        _segmentReaders.begin(), _segmentReaders.end(), size_t{0},
        [](size_t acc, auto const& r) { return acc + r.totalPos; });
    TRI_ASSERT(totalPos <= skipped);
    skipped -= std::min(skipped, totalPos);
  } else {
    auto const approximate = this->infos().countApproximate();
    // possible overfetch due to parallelisation
    skipped = this->_indexReadBuffer.size();
    for (auto& r : _segmentReaders) {
      if (r.itr) {
        skipped += calculateSkipAllCount(approximate, r.currentSegmentPos,
                                         r.itr.get());
        reset(r);
      }
    }
    auto& reader = _segmentReaders.front();
    while (_segmentOffset < count) {
      reader.readerOffset = _segmentOffset++;
      if (!resetIterator(reader)) {
        continue;
      }
      skipped += calculateSkipAllCount(approximate, 0, reader.itr.get());
      reset(reader);
    }
  }
  return skipped;
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::writeRow(ReadContext& ctx,
                                                      size_t idx) {
  auto const& value = this->_indexReadBuffer.getValue(idx);
  return Base::writeRowImpl(ctx, idx, value);
}

#include "IResearchViewExecutorInstantiations.h"
INSTANTIATE_VIEW_EXECUTOR(IResearchViewExecutor,
                          iresearch::MaterializeType::Materialize);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewExecutor,
                          iresearch::MaterializeType::NotMaterialize);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewExecutor,
                          iresearch::MaterializeType::LateMaterialize);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewExecutor,
                          iresearch::MaterializeType::NotMaterialize |
                              iresearch::MaterializeType::UseStoredValues);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewExecutor,
                          iresearch::MaterializeType::LateMaterialize |
                              iresearch::MaterializeType::UseStoredValues);

}  // namespace arangodb::aql
