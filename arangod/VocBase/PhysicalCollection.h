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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H
#define ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H 1

#include "Basics/Common.h"
#include "VocBase/DatafileDescription.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

struct TRI_datafile_t;
struct TRI_df_marker_t;

namespace arangodb {
class LogicalCollection;

class PhysicalCollection {
 protected:
  PhysicalCollection(LogicalCollection* collection) : _logicalCollection(collection) {}

 public:
  virtual ~PhysicalCollection() = default;

  virtual TRI_voc_rid_t revision() const = 0;
  
  // Used for Transaction rollback
  virtual void setRevision(TRI_voc_rid_t revision, bool force) = 0;
  
  virtual int64_t initialCount() const = 0;

  virtual void figures(std::shared_ptr<arangodb::velocypack::Builder>&) = 0;
  
  virtual int close() = 0;
  
  virtual int applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                                std::function<bool(TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker)> const& callback) = 0;

  /// @brief rotate the active journal - will do nothing if there is no journal
  virtual int rotateActiveJournal() = 0;
  
  /// @brief sync the active journal - will do nothing if there is no journal
  /// or if the journal is volatile
  virtual int syncActiveJournal() = 0;

  /// @brief reserve space in the current journal. if no create exists or the
  /// current journal cannot provide enough space, close the old journal and
  /// create a new one
  virtual int reserveJournalSpace(TRI_voc_tick_t tick, TRI_voc_size_t size,
                                  char*& resultPosition, TRI_datafile_t*& resultDatafile) = 0;
  
  /// @brief create compactor file
  virtual TRI_datafile_t* createCompactor(TRI_voc_fid_t fid, TRI_voc_size_t maximalSize) = 0;
  
  /// @brief close an existing compactor
  virtual int closeCompactor(TRI_datafile_t* datafile) = 0;
  
  /// @brief replace a datafile with a compactor
  virtual int replaceDatafileWithCompactor(TRI_datafile_t* datafile, TRI_datafile_t* compactor) = 0;
  
  virtual bool removeCompactor(TRI_datafile_t*) = 0;
  virtual bool removeDatafile(TRI_datafile_t*) = 0;
  
  /// @brief seal a datafile
  virtual int sealDatafile(TRI_datafile_t* datafile, bool isCompactor) = 0;
  
  /// @brief creates a datafile
  virtual TRI_datafile_t* createDatafile(TRI_voc_fid_t fid,
                                         TRI_voc_size_t journalSize, 
                                         bool isCompactor) = 0;

  /// @brief iterates over a collection
  virtual bool iterateDatafiles(std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb) = 0;

  virtual std::vector<DatafileDescription> datafilesInRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax) = 0;
  
 protected:
  LogicalCollection* _logicalCollection;
};

} // namespace arangodb

#endif
