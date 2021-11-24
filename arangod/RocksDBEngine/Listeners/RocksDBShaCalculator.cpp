////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBShaCalculator.h"

#include <algorithm>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabasePathFeature.h"

namespace arangodb {
  
RocksDBShaCalculatorThread::RocksDBShaCalculatorThread(application_features::ApplicationServer& server,
                                                       std::string const& name)
    : Thread(server, name) {}

// must call Thread::shutdown() in order to properly shutdown
RocksDBShaCalculatorThread::~RocksDBShaCalculatorThread() { shutdown(); }
  
template <typename T>
bool RocksDBShaCalculatorThread::isSstFilename(T const& filename) const {
  // length of  ".sst" is 4, but the actual filename should be something 
  // like xxxxxx.sst
  return filename.size() >= 4 && 
         (filename.compare(filename.size() - 4, 4, ".sst") == 0);
}

void RocksDBShaCalculatorThread::run() {
  try {
    // do an initial check over the entire directory first
    checkMissingShaFiles(getRocksDBPath(), 0);
        
    MUTEX_LOCKER(mutexLock, _pendingMutex);
    _lastFullCheck = std::chrono::steady_clock::now();
  } catch (...) {
  }

  while (!isStopping()) {
    try {
      MUTEX_LOCKER(mutexLock, _pendingMutex);
        
      // first check if we need to calculate any SHA values for
      // new sst files
      while (!_pendingCalculations.empty()) {
        std::string nextFile = _pendingCalculations.back();
        _pendingCalculations.pop_back();

        // check if a SHA calculation was requested for an sst file, 
        // but the file is already deleted by now
        if (_pendingDeletions.find(nextFile) == _pendingDeletions.end()) {
          // .sst file should still exist

          // continue without holding the mutex, so RocksDB can register
          // additional operations while we move on
          mutexLock.unlock();

          auto [ok, hash] = shaCalcFile(nextFile);
        
          mutexLock.lock();
        
          if (ok) {
            // store the calculated hash value for later
            TRI_ASSERT(nextFile != TRI_Basename(nextFile));
            _calculatedHashes[TRI_Basename(nextFile)] = std::move(hash);
          }
        }
      }

      // we are still holding the mutex here
      if (!_pendingDeletions.empty()) {
        mutexLock.unlock();

        deleteObsoleteFiles();
          
        mutexLock.lock();
      }

      bool runFullCheck = false;
      auto now = std::chrono::steady_clock::now();

      if (now >= _lastFullCheck + std::chrono::minutes(5)) {
        _lastFullCheck = now;
        runFullCheck = true;
      }

      mutexLock.unlock();

      if (runFullCheck) {
        // we could find files that subsequently post to _pendingOperations ... no worries.
        checkMissingShaFiles(getRocksDBPath(), 3 * 60);
        // need not to be written to in the past 3 minutes!
      }

      // no need for fast retry, hotbackups do not happen often
      CONDITION_LOCKER(condLock, _loopingCondvar);
      if (!isStopping()) {
        condLock.wait(std::chrono::seconds(30));
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("a27a1", ERR, arangodb::Logger::ENGINES)
          << "RocksDBShaCalculatorThread::run caught exception: " << ex.what();
    } catch (...) {
      LOG_TOPIC("66a10", ERR, arangodb::Logger::ENGINES)
          << "RocksDBShaCalculatorThread::run caught an exception";
    }
  }
}

void RocksDBShaCalculatorThread::queueShaCalcFile(std::string const& pathName) {
  if (isSstFilename(pathName)) {
    {
      MUTEX_LOCKER(lock, _pendingMutex);
      _pendingCalculations.emplace_back(pathName);
    }

    signalLoop();
  }
}

void RocksDBShaCalculatorThread::queueDeleteFile(std::string const& pathName) {
  if (isSstFilename(pathName)) {
    {
      MUTEX_LOCKER(lock, _pendingMutex);
      _pendingDeletions.emplace(pathName);
    }
    signalLoop();
  }
}

void RocksDBShaCalculatorThread::signalLoop() {
  CONDITION_LOCKER(lock, _loopingCondvar);
  lock.signal();
}

std::pair<bool, std::string> RocksDBShaCalculatorThread::shaCalcFile(std::string const& filename) {
  LOG_TOPIC("af088", DEBUG, arangodb::Logger::ENGINES)
      << "shaCalcFile: computing " << filename;
 
  TRI_SHA256Functor sha;
  if (TRI_ProcessFile(filename.c_str(), std::ref(sha))) {
    std::string hash = sha.finalize();
    std::string newfile = filename.substr(0, filename.size() - 4);
    newfile += ".sha.";
    newfile += hash;
    newfile += ".hash";
    LOG_TOPIC("80257", DEBUG, arangodb::Logger::ENGINES)
        << "shaCalcFile: done " << filename << " result: " << newfile;
    auto res = TRI_WriteFile(newfile.c_str(), "", 0);
    if (res == TRI_ERROR_NO_ERROR) {
      return {true, std::move(hash) }; 
    }

    LOG_TOPIC("8f7ef", DEBUG, arangodb::Logger::ENGINES)
        << "shaCalcFile: TRI_WriteFile failed with " << res << " for " << newfile;
  } else {
    LOG_TOPIC("7f3fd", DEBUG, arangodb::Logger::ENGINES)
        << "shaCalcFile:  TRI_ProcessFile failed for " << filename;
  }
    
  return {false, ""};
}

void RocksDBShaCalculatorThread::deleteObsoleteFiles() {
  std::string const dirname = getRocksDBPath();
  std::string scratch;

  MUTEX_LOCKER(mutexLock, _pendingMutex);

  while (!_pendingDeletions.empty()) {
    auto const& it = *_pendingDeletions.begin();
    // ".sst" is 4 characters
    TRI_ASSERT(isSstFilename(it));
    TRI_ASSERT(it.size() > 4);

    scratch.clear();

    auto it2 = _calculatedHashes.find(TRI_Basename(it));
    if (it2 != _calculatedHashes.end()) {
      // file names look like
      //   046440.sha.0dd3cc9fb90f6a32dd95ef721f7437ada30da588114a882284022123af414e8a.hash
      scratch.append(it.data(), it.size() - 4); // append without .sst
      TRI_ASSERT(!isSstFilename(scratch));
      scratch.append(".sha.");
      scratch.append((*it2).second);
      scratch.append(".hash");

      _calculatedHashes.erase(it2);
    }

    _pendingDeletions.erase(_pendingDeletions.begin());

    if (!scratch.empty()) {
      // we will remove this file from disk
      mutexLock.unlock();

      auto res = TRI_UnlinkFile(scratch.c_str());
      if (res == TRI_ERROR_NO_ERROR) {
        LOG_TOPIC("e0a0d", DEBUG, arangodb::Logger::ENGINES)
            << "deleteCalcFile:  TRI_UnlinkFile succeeded for " << scratch;
      } else {
        LOG_TOPIC("acb34", DEBUG, arangodb::Logger::ENGINES)
            << "deleteCalcFile:  TRI_UnlinkFile failed with " << res
            << " for " << scratch;
      }

      mutexLock.lock();
    }
  }
}

/// @brief Wrapper for getFeature<DatabasePathFeature> to simplify
///        unit testing
std::string RocksDBShaCalculatorThread::getRocksDBPath() {
  // get base path from DatabaseServerFeature
  auto& databasePathFeature = _server.getFeature<DatabasePathFeature>();
  return databasePathFeature.subdirectoryName("engine-rocksdb");
}

/// @brief Double check the active directory to see that all .sst files
///        have a matching .sha. (and delete any none matched .sha. files)
///        Will only consider .sst files which have not been written to for
///        `requireAge` seconds.
void RocksDBShaCalculatorThread::checkMissingShaFiles(std::string const& pathname,
                                                      int64_t requireAge) {
  std::vector<std::string> filelist = TRI_FilesDirectory(pathname.c_str());

  // sorting will put xxxxxx.sha.yyy just before xxxxxx.sst
  std::sort(filelist.begin(), filelist.end());

  std::string tempname;
      
  for (auto iter = filelist.begin(); filelist.end() != iter; ++iter) {
    if (iter->size() < 5) {
      // filename is too short and does not matter
      continue;
    }
      
    TRI_ASSERT(*iter == TRI_Basename(*iter));

    // cppcheck-suppress derefInvalidIteratorRedundantCheck
    std::string::size_type shaIdx = iter->find(".sha.");
    if (std::string::npos != shaIdx) {
      // two cases: 1. its .sst follows so skip both, 2. no matching .sst, so delete
      tempname = iter->substr(0, shaIdx) + ".sst";
      TRI_ASSERT(tempname == TRI_Basename(tempname));

      // cppcheck-suppress derefInvalidIteratorRedundantCheck
      auto next = iter + 1;
      // cppcheck-suppress derefInvalidIteratorRedundantCheck
      if (filelist.end() != next && 0 == next->compare(tempname)) {
        TRI_ASSERT(iter->size() >= shaIdx + 64);
        std::string hash = iter->substr(shaIdx + /*.sha.*/ 5, 64);
        
        iter = next;

        // update our hashes table, in case we missed this file
        MUTEX_LOCKER(mutexLock, _pendingMutex);
        _calculatedHashes[tempname] = std::move(hash);
      } else {
        std::string tempPath = basics::FileUtils::buildFilename(pathname, *iter);
        LOG_TOPIC("4eac9", DEBUG, arangodb::Logger::ENGINES)
            << "checkMissingShaFiles:"
               " Deleting file "
            << tempPath;
        TRI_UnlinkFile(tempPath.c_str());
          
        // remove from our calculated hashes map
        MUTEX_LOCKER(mutexLock, _pendingMutex);
        _calculatedHashes.erase(tempname);
      }
    } else if (iter->size() > 4 &&
               iter->compare(iter->size() - 4, 4, ".sst") == 0) {
      // we only get here if we found an .sst find but no .sha file directly
      // in front of it!
      MUTEX_LOCKER(mutexLock, _pendingMutex);

      auto it2 = _calculatedHashes.find(*iter);
      if (it2 == _calculatedHashes.end()) {
        // not yet calculated
        mutexLock.unlock();

        // reaching this point means no .sha. preceeded, now check the
        // modification time, if younger than a few mins, just leave it, otherwise,
        // we create a sha file. This is to ensure that sha files are eventually
        // generated if somebody switches on backup after the fact. However,
        // normally, the shas should only be computed when the sst file has
        // been fully written, which can only be guaranteed if we got a
        // creation event.
        std::string tempPath = basics::FileUtils::buildFilename(pathname, *iter);
        int64_t now = ::time(nullptr);
        int64_t modTime = 0;
        auto r = TRI_MTimeFile(tempPath.c_str(), &modTime);
        if (r == TRI_ERROR_NO_ERROR && (now - modTime) >= requireAge) {
          LOG_TOPIC("d6c86", DEBUG, arangodb::Logger::ENGINES)
              << "checkMissingShaFiles:"
                 " Computing checksum for "
              << tempPath;

          // calculate hash value and generate .hash file
          auto [ok, hash] = shaCalcFile(tempPath);
          
          if (ok) {
            MUTEX_LOCKER(mutexLock, _pendingMutex);
            _calculatedHashes[*iter] = std::move(hash);
          }
        } else {
          LOG_TOPIC("7f70f", DEBUG, arangodb::Logger::ENGINES)
              << "checkMissingShaFiles:"
                 " Not computing checksum for "
              << tempPath << " since it is too young";
        }
      }
    }
  }
}

/// @brief Setup the object, clearing variables, but do no real work
RocksDBShaCalculator::RocksDBShaCalculator(application_features::ApplicationServer& server,
                                           bool startThread)
    : _shaThread(server, "Sha256Thread") {
  startThread = false;
  if (startThread) {
    _shaThread.start(&_threadDone);
  }
}

/// @brief Shutdown the background thread only if it was ever started
RocksDBShaCalculator::~RocksDBShaCalculator() {
  // when we get here the background thread should have been stopped
  // already.
  TRI_ASSERT(!_shaThread.hasStarted() || !_shaThread.isRunning());
  waitForShutdown();
}
  
void RocksDBShaCalculator::waitForShutdown() {
  // send shutdown signal to SHA thread
  _shaThread.beginShutdown();
  
  _shaThread.signalLoop();

  CONDITION_LOCKER(locker, _threadDone);
  if (_shaThread.hasStarted() && _shaThread.isRunning()) {
    _threadDone.wait();
  }
  // now we are sure the SHA thread is not running anymore
}

void RocksDBShaCalculator::OnFlushCompleted(rocksdb::DB* db,
                                            const rocksdb::FlushJobInfo& flush_job_info) {
  _shaThread.queueShaCalcFile(flush_job_info.file_path);
}

void RocksDBShaCalculator::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo& info) {
  _shaThread.queueDeleteFile(info.file_path);
}

void RocksDBShaCalculator::OnCompactionCompleted(rocksdb::DB* db,
                                                 const rocksdb::CompactionJobInfo& ci) {
  for (auto const& filename : ci.output_files) {
    _shaThread.queueShaCalcFile(filename);
  }
}

void RocksDBShaCalculator::checkMissingShaFiles(std::string const& pathname, int64_t requireAge) {
  _shaThread.checkMissingShaFiles(pathname, requireAge);
}

}  // namespace arangodb
