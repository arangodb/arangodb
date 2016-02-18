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

#ifndef ARANGOD_VOC_BASE_REPLICATION_DUMP_H
#define ARANGOD_VOC_BASE_REPLICATION_DUMP_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "VocBase/replication-common.h"
#include "VocBase/shaped-json.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

struct TRI_shape_s;
class TRI_vocbase_col_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief replication dump container
////////////////////////////////////////////////////////////////////////////////

struct TRI_replication_dump_t {
  TRI_replication_dump_t(TRI_vocbase_t* vocbase, size_t chunkSize,
                         bool includeSystem, TRI_voc_cid_t restrictCollection)
      : _vocbase(vocbase),
        _buffer(nullptr),
        _chunkSize(chunkSize),
        _lastFoundTick(0),
        _lastSid(0),
        _lastShape(nullptr),
        _restrictCollection(restrictCollection),
        _collectionNames(),
        _failed(false),
        _bufferFull(false),
        _hasMore(false),
        _includeSystem(includeSystem),
        _fromTickIncluded(false) {
    if (_chunkSize == 0) {
      // default chunk size
      _chunkSize = 128 * 1024;
    }

    _buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, _chunkSize);

    if (_buffer == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  ~TRI_replication_dump_t() {
    if (_buffer != nullptr) {
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, _buffer);
      _buffer = nullptr;
    }
  }

  TRI_vocbase_t* _vocbase;
  TRI_string_buffer_t* _buffer;
  size_t _chunkSize;
  TRI_voc_tick_t _lastFoundTick;
  TRI_shape_sid_t _lastSid;
  struct TRI_shape_s const* _lastShape;
  TRI_voc_cid_t _restrictCollection;
  std::unordered_map<TRI_voc_cid_t, std::string> _collectionNames;
  bool _failed;
  bool _bufferFull;
  bool _hasMore;
  bool _includeSystem;
  bool _fromTickIncluded;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from a single collection
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpCollectionReplication(TRI_replication_dump_t*, TRI_vocbase_col_t*,
                                  TRI_voc_tick_t, TRI_voc_tick_t, bool, bool,
                                  bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from the replication log
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpLogReplication(TRI_replication_dump_t*,
                           std::unordered_set<TRI_voc_tid_t> const&,
                           TRI_voc_tick_t, TRI_voc_tick_t, TRI_voc_tick_t,
                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the transactions that were open at a given point in time
////////////////////////////////////////////////////////////////////////////////

int TRI_DetermineOpenTransactionsReplication(TRI_replication_dump_t*,
                                             TRI_voc_tick_t, TRI_voc_tick_t);

#endif
