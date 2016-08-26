////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_WAL_COLLECTOR_CACHE_H
#define ARANGOD_WAL_COLLECTOR_CACHE_H 1

#include "Basics/Common.h"
#include "VocBase/DatafileStatisticsContainer.h"
#include "VocBase/Ditch.h"
#include "VocBase/voc-types.h"

struct TRI_datafile_t;
struct TRI_df_marker_t;

namespace arangodb {

namespace wal {

class Logfile;

struct CollectorOperation {
  CollectorOperation(char const* datafilePosition,
                     TRI_voc_size_t datafileMarkerSize, char const* walPosition,
                     TRI_voc_fid_t datafileId)
      : datafilePosition(datafilePosition),
        datafileMarkerSize(datafileMarkerSize),
        walPosition(walPosition),
        datafileId(datafileId) {
    TRI_ASSERT(datafilePosition != nullptr);
    TRI_ASSERT(datafileMarkerSize > 0);
    TRI_ASSERT(walPosition != nullptr);
    TRI_ASSERT(datafileId > 0);
  }

  char const* datafilePosition;
  TRI_voc_size_t datafileMarkerSize;
  char const* walPosition;
  TRI_voc_fid_t datafileId;
};

struct CollectorCache {
  CollectorCache(CollectorCache const&) = delete;
  CollectorCache& operator=(CollectorCache const&) = delete;

  CollectorCache(TRI_voc_cid_t collectionId, TRI_voc_tick_t databaseId,
                 Logfile* logfile, int64_t totalOperationsCount,
                 size_t operationsSize)
      : collectionId(collectionId),
        databaseId(databaseId),
        logfile(logfile),
        totalOperationsCount(totalOperationsCount),
        operations(new std::vector<CollectorOperation>()),
        ditches(),
        dfi(),
        lastFid(0),
        lastDatafile(nullptr) {
    operations->reserve(operationsSize);
  }

  ~CollectorCache() {
    delete operations;
    freeDitches();
  }

  /// @brief return a reference to an existing datafile statistics struct
  DatafileStatisticsContainer& getDfi(TRI_voc_fid_t fid) {
    return dfi[fid];
  }

  /// @brief return a reference to an existing datafile statistics struct,
  /// create it if it does not exist
  DatafileStatisticsContainer& createDfi(TRI_voc_fid_t fid) {
    auto it = dfi.find(fid);

    if (it != dfi.end()) {
      return (*it).second;
    }

    dfi.emplace(fid, DatafileStatisticsContainer());

    return dfi[fid];
  }

  /// @brief add a ditch
  void addDitch(arangodb::DocumentDitch* ditch) {
    TRI_ASSERT(ditch != nullptr);
    ditches.emplace_back(ditch);
  }

  /// @brief free all ditches
  void freeDitches() {
    for (auto& it : ditches) {
      it->ditches()->freeDocumentDitch(it, false);
    }

    ditches.clear();
  }

  /// @brief id of collection
  TRI_voc_cid_t const collectionId;

  /// @brief id of database
  TRI_voc_tick_t const databaseId;

  /// @brief id of the WAL logfile
  Logfile* logfile;

  /// @brief total number of operations in this block
  int64_t const totalOperationsCount;

  /// @brief all collector operations of a collection
  std::vector<CollectorOperation>* operations;

  /// @brief ditches held by the operations
  std::vector<arangodb::DocumentDitch*> ditches;

  /// @brief datafile info cache, updated when the collector transfers markers
  std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer> dfi;

  /// @brief id of last datafile handled
  TRI_voc_fid_t lastFid;

  /// @brief last datafile written to
  TRI_datafile_t* lastDatafile;
};
  
/// @brief typedef key => document marker
typedef std::unordered_map<std::string, struct TRI_df_marker_t const*>
    DocumentOperationsType;

/// @brief typedef for structural operation (attributes, shapes) markers
typedef std::vector<struct TRI_df_marker_t const*> OperationsType;

}
}

#endif
