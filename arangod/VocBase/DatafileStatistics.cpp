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

#include "DatafileStatistics.h"
#include "Basics/Exceptions.h"
#include "Basics/Logger.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "VocBase/datafile.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create an empty datafile statistics container
////////////////////////////////////////////////////////////////////////////////

DatafileStatisticsContainer::DatafileStatisticsContainer()
    : numberAlive(0),
      numberDead(0),
      numberDeletions(0),
      numberShapes(0),
      numberAttributes(0),
      sizeAlive(0),
      sizeDead(0),
      sizeShapes(0),
      sizeAttributes(0),
      numberUncollected(0) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief update statistics from another container
////////////////////////////////////////////////////////////////////////////////

void DatafileStatisticsContainer::update(
    DatafileStatisticsContainer const& other) {
  numberAlive += other.numberAlive;
  numberDead += other.numberDead;
  numberDeletions += other.numberDeletions;
  numberShapes += other.numberShapes;
  numberAttributes += other.numberAttributes;
  sizeAlive += other.sizeAlive;
  sizeDead += other.sizeDead;
  sizeShapes += other.sizeShapes;
  sizeAttributes += other.sizeAttributes;
  numberUncollected += other.numberUncollected;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the statistics values
////////////////////////////////////////////////////////////////////////////////

void DatafileStatisticsContainer::reset() {
  numberAlive = 0;
  numberDead = 0;
  numberDeletions = 0;
  numberShapes = 0;
  numberAttributes = 0;
  sizeAlive = 0;
  sizeDead = 0;
  sizeShapes = 0;
  sizeAttributes = 0;
  numberUncollected = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create statistics manager for a collection
////////////////////////////////////////////////////////////////////////////////

DatafileStatistics::DatafileStatistics() : _lock(), _stats() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy statistics manager
////////////////////////////////////////////////////////////////////////////////

DatafileStatistics::~DatafileStatistics() {
  WRITE_LOCKER(writeLocker, _lock);

  for (auto& it : _stats) {
    delete it.second;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an empty statistics container for a file
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::create(TRI_voc_fid_t fid) {
  auto stats = std::make_unique<DatafileStatisticsContainer>();

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it != _stats.end()) {
    // already exists
    return;
  }

  LOG(TRACE) << "creating statistics for datafile " << fid;
  _stats.emplace(fid, stats.get());
  stats.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create statistics for a datafile, using the stats provided
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::create(TRI_voc_fid_t fid,
                                DatafileStatisticsContainer const& src) {
  auto stats = std::make_unique<DatafileStatisticsContainer>();
  *stats = src;

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it != _stats.end()) {
    // already exists
    return;
  }

  LOG(TRACE) << "creating statistics for datafile " << fid << " from initial data";

  _stats.emplace(fid, stats.get());
  stats.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove statistics for a file
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::remove(TRI_voc_fid_t fid) {
  LOG(TRACE) << "removing statistics for datafile " << fid;

  DatafileStatisticsContainer* found = nullptr;
  {
    WRITE_LOCKER(writeLocker, _lock);

    auto it = _stats.find(fid);

    if (it != _stats.end()) {
      found = (*it).second;    
      _stats.erase(it);
    }
  }
 
  delete found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge statistics for a file
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::update(TRI_voc_fid_t fid,
                                DatafileStatisticsContainer const& src) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    LOG(WARN) << "did not find required statistics for datafile " << fid;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "required datafile statistics not found");
  }

  auto& dst = (*it).second;

  LOG(TRACE) << "updating statistics for datafile " << fid;
  dst->update(src);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge statistics for a file, by copying the stats from another
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::update(TRI_voc_fid_t fid, TRI_voc_fid_t src) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    LOG(WARN) << "did not find required statistics for datafile " << fid;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "required datafile statistics not found");
  }

  auto& dst = (*it).second;

  it = _stats.find(src);

  if (it == _stats.end()) {
    LOG(WARN) << "did not find required statistics for source datafile " << src;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "required datafile statistics not found");
  }

  LOG(TRACE) << "updating statistics for datafile " << fid;
  dst->update(*(*it).second);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace statistics for a file
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::replace(TRI_voc_fid_t fid,
                                 DatafileStatisticsContainer const& src) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    LOG(WARN) << "did not find required statistics for datafile " << fid;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "required datafile statistics not found");
  }

  auto& dst = (*it).second;
  *dst = src;

  LOG(TRACE) << "replacing statistics for datafile " << fid;
}

/////////////////////////////////////////////////////////////////////////////////
/// @brief increase dead stats for a datafile, if it exists
/////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::increaseDead(TRI_voc_fid_t fid, int64_t number,
                                      int64_t size) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    // datafile not found. no problem
    return;
  }

  auto& dst = (*it).second;
  dst->numberDead += number;
  dst->sizeDead += size;
  dst->numberAlive -= number;
  dst->sizeAlive -= size;
}

/////////////////////////////////////////////////////////////////////////////////
/// @brief increase number of uncollected entries
/////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::increaseUncollected(TRI_voc_fid_t fid,
                                             int64_t number) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    // datafile not found. no problem
    return;
  }

  auto& dst = (*it).second;
  dst->numberUncollected += number;

  LOG(TRACE) << "increasing uncollected count for datafile " << fid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a copy of the datafile statistics for a file
////////////////////////////////////////////////////////////////////////////////

DatafileStatisticsContainer DatafileStatistics::get(TRI_voc_fid_t fid) {
  DatafileStatisticsContainer result;
  {
    READ_LOCKER(readLocker, _lock);

    auto it = _stats.find(fid);

    if (it == _stats.end()) {
      LOG(WARN) << "did not find required statistics for datafile " << fid;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "required datafile statistics not found");
    }

    result = *(*it).second;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a copy of the datafile statistics for a file
////////////////////////////////////////////////////////////////////////////////

DatafileStatisticsContainer DatafileStatistics::all() {
  DatafileStatisticsContainer result;
  {
    READ_LOCKER(readLocker, _lock);

    for (auto& it : _stats) {
      result.update(*(it.second));
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a journal
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::addJournal(TRI_datafile_t* df) {
  WRITE_LOCKER(writeLocker, _lock);

  TRI_ASSERT(_journals.empty());
  _journals.push_back(df);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a datafile
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::addDatafile(TRI_datafile_t* df) {
  WRITE_LOCKER(writeLocker, _lock);
  _datafiles.push_back(df);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a compactor
////////////////////////////////////////////////////////////////////////////////

void DatafileStatistics::addCompactor(TRI_datafile_t* df) {
  WRITE_LOCKER(writeLocker, _lock);

  TRI_ASSERT(_compactors.empty());
  _compactors.push_back(df);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if there's a compactor
////////////////////////////////////////////////////////////////////////////////

bool DatafileStatistics::hasCompactor() {
  READ_LOCKER(readLocker, _lock);

  return !_compactors.empty();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief turn a compactor into a datafile
////////////////////////////////////////////////////////////////////////////////

bool DatafileStatistics::compactorToDatafile(TRI_datafile_t* df) {
  WRITE_LOCKER(writeLocker, _lock);

  for (auto it = _compactors.begin(); it != _compactors.end(); ++it) {
    if ((*it) == df) {
      // if the following fails, we just throw, but no harm is done
      _datafiles.push_back(df);

      // and finally remove the file from the _compactors vector
      _compactors.erase(it);
      return true;
    }
  }

  // not found
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief turn a journal into a datafile
////////////////////////////////////////////////////////////////////////////////

bool DatafileStatistics::journalToDatafile(TRI_datafile_t* df) {
  WRITE_LOCKER(writeLocker, _lock);

  for (auto it = _journals.begin(); it != _journals.end(); ++it) {
    if ((*it) == df) {
      // if the following fails, we just throw, but no harm is done
      _datafiles.push_back(df);

      // and finally remove the file from the _compactors vector
      _journals.erase(it);
      return true;
    }
  }

  // not found
  return false;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief remove a compactor file
//////////////////////////////////////////////////////////////////////////////

bool DatafileStatistics::removeCompactor(TRI_datafile_t* df) {
  WRITE_LOCKER(writeLocker, _lock);

  for (auto it = _compactors.begin(); it != _compactors.end(); ++it) {
    if ((*it) == df) {
      // and finally remove the file from the _compactors vector
      _compactors.erase(it);
      return true;
    }
  }

  // not found
  return false;
}
