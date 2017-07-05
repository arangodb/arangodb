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

#ifndef ARANGOD_MMFILES_MMFILES_REPLICATION_DUMP_H
#define ARANGOD_MMFILES_MMFILES_REPLICATION_DUMP_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/replication-common.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

#include <vector>

/// @brief replication dump container
struct MMFilesReplicationDumpContext {
  MMFilesReplicationDumpContext(std::shared_ptr<arangodb::transaction::StandaloneContext> const&
                             transactionContext,
                         size_t chunkSize, bool includeSystem,
                         TRI_voc_cid_t restrictCollection, bool useVst = false)
      : _transactionContext(transactionContext),
        _vocbase(transactionContext->vocbase()),
        _buffer(nullptr),
        _chunkSize(chunkSize),
        _lastFoundTick(0),
        _restrictCollection(restrictCollection),
        _collectionNames(),
        _vpackOptions(arangodb::velocypack::Options::Defaults),
        _failed(false),
        _bufferFull(false),
        _hasMore(false),
        _includeSystem(includeSystem),
        _fromTickIncluded(false),
        _compat28(false),
        _slices(),
        _useVst(useVst) {
    if (_chunkSize == 0) {
      // default chunk size
      _chunkSize = 128 * 1024;
    }

    if (!useVst) {
      _buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, _chunkSize);

      if (_buffer == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }
  }

  ~MMFilesReplicationDumpContext() {
    if (_buffer != nullptr) {
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, _buffer);
      _buffer = nullptr;
    }
  }

  std::shared_ptr<arangodb::transaction::StandaloneContext> _transactionContext;
  TRI_vocbase_t* _vocbase;
  TRI_string_buffer_t* _buffer;
  size_t _chunkSize;
  TRI_voc_tick_t _lastFoundTick;
  TRI_voc_cid_t _restrictCollection;
  std::unordered_map<TRI_voc_cid_t, std::string> _collectionNames;
  arangodb::velocypack::Options _vpackOptions;
  bool _failed;
  bool _bufferFull;
  bool _hasMore;
  bool _includeSystem;
  bool _fromTickIncluded;
  bool _compat28;
  std::vector<VPackBuffer<uint8_t>> _slices;
  bool _useVst;
};

/// @brief dump data from a single collection
int MMFilesDumpCollectionReplication(MMFilesReplicationDumpContext*,
                                  arangodb::LogicalCollection*, TRI_voc_tick_t,
                                  TRI_voc_tick_t, bool);

/// @brief dump data from the replication log
int MMFilesDumpLogReplication(MMFilesReplicationDumpContext*,
                           std::unordered_set<TRI_voc_tid_t> const&,
                           TRI_voc_tick_t, TRI_voc_tick_t, TRI_voc_tick_t,
                           bool);

/// @brief determine the transactions that were open at a given point in time
int MMFilesDetermineOpenTransactionsReplication(MMFilesReplicationDumpContext*,
                                             TRI_voc_tick_t, TRI_voc_tick_t,
                                             bool useVst = false);

#endif
