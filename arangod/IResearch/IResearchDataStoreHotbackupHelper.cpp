////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/IResearchDataStoreHotbackupHelper.h"

#include "IResearch/IResearchCommon.h"

#include <index/column_info.hpp>
#include <store/caching_directory.hpp>
#include <store/mmap_directory.hpp>
#include <store/store_utils.hpp>
#include <utils/encryption.hpp>
#include <utils/singleton.hpp>

namespace arangodb::iresearch {
namespace {

enum class SegmentPayloadVersion : uint32_t {
  // Only low tick stored.
  // Possibly has committed WAL records after it, but mostly uncommitted.
  SingleTick = 0,
  // Two stage commit ticks are stored.
  // low tick is fully committed but between low and high tick
  // uncommitted WAL records may be present.
  // After high tick nothing is committed.
  TwoStageTick = 1
};

bool readTick(irs::bytes_view payload, uint64_t& tickLow,
              uint64_t& tickHigh) noexcept {
  if (payload.size() < sizeof(uint64_t)) {
    LOG_TOPIC("41474", ERR, TOPIC) << "Unexpected segment payload size "
                                   << payload.size() << " for initial check";
    return false;
  }
  std::memcpy(&tickLow, payload.data(), sizeof(uint64_t));
  tickLow = irs::numeric_utils::ntoh64(tickLow);
  tickHigh = std::numeric_limits<uint64_t>::max();
  using Version = std::underlying_type_t<SegmentPayloadVersion>;
  Version version;
  if (payload.size() < sizeof(uint64_t) + sizeof(version)) {
    return true;
  }
  std::memcpy(&version, payload.data() + sizeof(uint64_t), sizeof(version));
  version = irs::numeric_utils::ntoh32(version);
  switch (version) {
    case (static_cast<Version>(SegmentPayloadVersion::TwoStageTick)): {
      if (payload.size() == (2 * sizeof(uint64_t)) + sizeof(version)) {
        std::memcpy(&tickHigh,
                    payload.data() + sizeof(uint64_t) + sizeof(version),
                    sizeof(uint64_t));
        tickHigh = irs::numeric_utils::ntoh64(tickHigh);
      } else {
        LOG_TOPIC("49b4d", ERR, TOPIC)
            << "Unexpected segment payload size " << payload.size()
            << " for version '" << version << "'";
      }
    } break;
    default:
      // falling back to SingleTick as it always present
      LOG_TOPIC("fad1f", WARN, TOPIC)
          << "Unexpected segment payload version '" << version
          << "' fallback to minimal version";
  }
  return true;
}

}  // namespace

arangodb::Result IResearchDataStoreHotbackupHelper::initDataStore(
    std::string path, uint32_t version, bool sorted, bool nested,
    std::span<const IResearchViewStoredValues::StoredColumn> storedColumns,
    irs::type_info::type_id primarySortCompression,
    irs::IndexReaderOptions const& readerOptions) {
  // The data-store is being deallocated, link use is no longer valid
  // (wait for all the view users to finish)
  // _asyncSelf->reset();
  // Reset together with '_asyncSelf'
  // _flushSubscription.reset();

  _hasNestedFields = nested;

  /// ???
  auto const formatId = getFormat(LinkVersion{version});
  auto format = irs::formats::get(formatId);
  ADB_PROD_ASSERT(format != nullptr)
      << absl::StrCat("Failed to get data store codec '", formatId,
                      "' while initializing ArangoSearch index");

  _dataStore._path = path;

  // Must manually ensure that the data store directory exists
  // (since not using a lockfile)
  std::error_code error;
  auto pathExists = std::filesystem::exists(_dataStore._path, error);
  if (!pathExists) {
    ADB_PROD_ASSERT(false) << "BOOM";
  }

  // Thes initCallback is needed (ref to IResearchDataSTore.cpp) to setup
  // encryption of the store
  _dataStore._directory = std::make_unique<irs::MMapDirectory>(
      _dataStore._path, irs::directory_attributes{});

  // TODO: is this relevant for applying WAL later?
  _lastCommittedTick = _dataStore._recoveryTickLow;

  auto const openMode = (irs::OM_CREATE | irs::OM_APPEND);
  auto const options = getWriterOptions(readerOptions, version, sorted, nested,
                                        storedColumns, primarySortCompression);
  _dataStore._writer = irs::IndexWriter::Make(
      *(_dataStore._directory), std::move(format), openMode, options);
  TRI_ASSERT(_dataStore._writer);

  auto reader = _dataStore._writer->GetSnapshot();
  TRI_ASSERT(reader);

  if (!readTick(irs::GetPayload(reader.Meta().index_meta),
                _dataStore._recoveryTickLow, _dataStore._recoveryTickHigh)) {
    ADB_PROD_ASSERT(false) << "TODO: Proper error handling";
    _lastCommittedTick = _dataStore._recoveryTickLow;
  }

#if 0
  LOG_TOPIC("f00af", DEBUG, TOPIC)
      << toString(reader, !pathExists, index().id(),
                  _dataStore._recoveryTickLow, _dataStore._recoveryTickHigh);
#endif

  // Reset data store meta, will be updated at runtime via properties(...)
  _dataStore._meta._cleanupIntervalStep = 0;        // 0 == disable
  _dataStore._meta._commitIntervalMsec = 0;         // 0 == disable
  _dataStore._meta._consolidationIntervalMsec = 0;  // 0 == disable
  _dataStore._meta._consolidationPolicy =
      IResearchDataStoreMeta::ConsolidationPolicy();  // disable
  _dataStore._meta._writebufferActive = options.segment_count_max;
  _dataStore._meta._writebufferIdle = options.segment_pool_size;
  _dataStore._meta._writebufferSizeMax = options.segment_memory_max;

  _asyncSelf = std::make_shared<AsyncLinkHandle>(this);

  // TODO: mpf is this needed?
  _recoveryRemoves = makePrimaryKeysFilter(_hasNestedFields);
  _recoveryTrx = _dataStore._writer->GetBatch();

  return {};
}

}  // namespace arangodb::iresearch
