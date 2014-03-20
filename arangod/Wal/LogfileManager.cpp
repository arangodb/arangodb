////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log logfile manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "LogfileManager.h"
#include "BasicsC/hashes.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/JsonHelper.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "VocBase/server.h"
#include "Wal/AllocatorThread.h"
#include "Wal/CollectorThread.h"
#include "Wal/Configuration.h"
#include "Wal/Slots.h"
#include "Wal/SynchroniserThread.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the logfile manager
////////////////////////////////////////////////////////////////////////////////

LogfileManager::LogfileManager (Configuration* configuration) 
  : _configuration(configuration),
    _slots(nullptr),
    _synchroniserThread(nullptr),
    _allocatorThread(nullptr),
    _collectorThread(nullptr),
    _logfilesLock(),
    _lastCollectedId(0),
    _logfiles(),
    _maxEntrySize(configuration->maxEntrySize()),
    _directory(configuration->directory()),
    _regex(),
    _shutdown(false) {
  
  LOG_INFO("creating wal logfile manager");

  int res = regcomp(&_regex, "^logfile-([0-9][0-9]*)\\.db$", REG_EXTENDED);

  if (res != 0) {
    THROW_INTERNAL_ERROR("could not compile regex"); 
  }

  _slots = new Slots(this, 32, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the logfile manager
////////////////////////////////////////////////////////////////////////////////

LogfileManager::~LogfileManager () {
  LOG_INFO("shutting down wal logfile manager");

  shutdown();

  regfree(&_regex);

  if (_slots != nullptr) {
    delete _slots;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not there are reserve logfiles
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::hasReserveLogfiles () {
  size_t numberOfLogfiles = 0;

  READ_LOCKER(_logfilesLock);
      
  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr && 
      logfile->freeSize() > 0 && 
      ! logfile->isSealed()) {

      if (++numberOfLogfiles >= 3) { // TODO: make "3" a configuration option
        return true;
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startup the logfile manager
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startup () {
  int res = inventory();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not create wal logfile inventory: %s", TRI_errno_string(res));
    return res;
  }
 
  std::string const shutdownFile = shutdownFilename();
  bool const shutdownFileExists = basics::FileUtils::exists(shutdownFile); 

  if (shutdownFileExists) {
    res = readShutdownInfo();
  
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("could not open shutdown file '%s': %s", shutdownFile.c_str(), TRI_errno_string(res));
      return res;
    }
  }

  res = openLogfiles();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not open wal logfiles: %s", TRI_errno_string(res));
    return res;
  }

  res = startSynchroniserThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start wal synchroniser thread: %s", TRI_errno_string(res));
    return res;
  }

  res = startAllocatorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start wal allocator thread: %s", TRI_errno_string(res));
    return res;
  }
  
  res = startCollectorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start wal collector thread: %s", TRI_errno_string(res));
    return res;
  }

  if (shutdownFileExists) {
    // delete the shutdown file if it existed
    if (! basics::FileUtils::remove(shutdownFile, &res)) {
      LOG_ERROR("could not remove shutdown file '%s': %s", shutdownFile.c_str(), TRI_errno_string(res));
      return res;
    }
  }

  LOG_INFO("wal logfile manager started successfully");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down and closes all open logfiles
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::shutdown () {
  if (_shutdown) {
    return;
  }

  _shutdown = true;

  LOG_INFO("stopping collector thread");
  // stop threads
  stopCollectorThread();
  
  LOG_INFO("stopping allocator thread");
  stopAllocatorThread();
  
  LOG_INFO("stopping synchroniser thread");
  stopSynchroniserThread();
  
  LOG_INFO("closing logfiles");
  sleep(1);

  // close all open logfiles
  closeLogfiles();

  int res = writeShutdownInfo();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not write wal shutdown info: %s", TRI_errno_string(res));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signal that a sync operation is required
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::signalSync () {
  _synchroniserThread->signalSync();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate space in a logfile for later writing
////////////////////////////////////////////////////////////////////////////////

Slot* LogfileManager::allocate (uint32_t size, int ctx) {
  /*
  if (size > _maxEntrySize) {
    // entry is too big
    return LogEntry(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }
  */

  return _slots->nextUnused(size, ctx);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finalise a log entry
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::finalise (Slot* slot) {
  _slots->returnUsed(slot);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data into the logfile
/// this is a convenience function that combines allocate, memcpy and finalise
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::allocateAndWrite (void* mem,
                                      uint32_t size,
                                      int ctx) {

  Slot* slot = allocate(size, ctx);
 
  if (slot == nullptr) {
    // TODO: return "real" error code
    LOG_ERROR("no free slot!");
    assert(false);
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  TRI_df_marker_t* marker = static_cast<TRI_df_marker_t*>(slot->mem());

  // write tick into marker
  marker->_tick = slot->tick();

  // set initial crc to 0
  marker->_crc = 0;
  
  // now calculate crc  
  TRI_voc_crc_t crc = TRI_InitialCrc32();
  crc = TRI_BlockCrc32(crc, static_cast<char const*>(mem), static_cast<TRI_voc_size_t>(size));
  marker->_crc = TRI_FinalCrc32(crc);

  memcpy(slot->mem(), mem, static_cast<TRI_voc_size_t>(size));

  return finalise(slot);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief seal a logfile
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::sealLogfile (Logfile* logfile) {
  assert(logfile != nullptr);

  LOG_INFO("sealing logfile");

  WRITE_LOCKER(_logfilesLock);
  logfile->seal();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile for writing
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getWriteableLogfile (uint32_t size) {
  size_t iterations = 0;

  while (++iterations < 1000) {
    {
      READ_LOCKER(_logfilesLock);

      for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
        Logfile* logfile = (*it).second;

        if (logfile != nullptr && 
            logfile->isWriteable(size)) {
          return logfile;
        }
      }
    }

    // signal & sleep outside the lock
    _allocatorThread->signalLogfileCreation();
    usleep(10000);
  }
  
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile to collect. this may return nullptr
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getCollectableLogfile () {
  READ_LOCKER(_logfilesLock);

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr && logfile->canCollect()) {
      return logfile;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a file as being requested for collection
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setCollectionRequested (Logfile* logfile) {
  assert(logfile != nullptr);

  WRITE_LOCKER(_logfilesLock);
  logfile->setCollectionRequested();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a file as being done with collection
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setCollectionDone (Logfile* logfile) {
  assert(logfile != nullptr);

  {
    WRITE_LOCKER(_logfilesLock);
    logfile->setCollectionDone();
  
    // finally get rid of the logfile
    _logfiles.erase(logfile->id());
  }

  delete logfile;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all logfiles
////////////////////////////////////////////////////////////////////////////////
  
void LogfileManager::closeLogfiles () {
  WRITE_LOCKER(_logfilesLock);

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr) {
      delete logfile;
    }
  }

  _logfiles.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the id of the last fully collected logfile
/// returns 0 if no logfile was yet collected or no information about the
/// collection is present
////////////////////////////////////////////////////////////////////////////////

Logfile::IdType LogfileManager::lastCollected () {
  READ_LOCKER(_logfilesLock);
  return _lastCollectedId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads the shutdown information
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::readShutdownInfo () {
  std::string const filename = shutdownFilename();

  TRI_json_t* json = TRI_JsonFile(TRI_UNKNOWN_MEM_ZONE, filename.c_str(), nullptr); 

  if (json == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  // read last assigned tick (may be 0)
  uint64_t const lastTick = basics::JsonHelper::stringUInt64(json, "lastTick");
  _slots->setLastTick(static_cast<Slot::TickType>(lastTick));
  
  // read if of last collected logfile (maybe 0)
  uint64_t const lastCollected = basics::JsonHelper::stringUInt64(json, "lastCollected");
  
  {
    WRITE_LOCKER(_logfilesLock);
    _lastCollectedId = static_cast<Logfile::IdType>(lastCollected);
  }
  
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  return TRI_ERROR_NO_ERROR; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the shutdown information
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::writeShutdownInfo () {
  std::string const filename = shutdownFilename();

  std::string content;
  content.append("{\"lastTick\":\"");
  content.append(basics::StringUtils::itoa(_slots->lastTick()));
  content.append("\",\"lastCollected\":\"");
  content.append(basics::StringUtils::itoa(lastCollected()));
  content.append("\"}");

  // TODO: spit() doesn't return success/failure. FIXME!
  basics::FileUtils::spit(filename, content);
  
  return TRI_ERROR_NO_ERROR; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startSynchroniserThread () {
  _synchroniserThread = new SynchroniserThread(this);

  if (_synchroniserThread == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _synchroniserThread->start()) {
    delete _synchroniserThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stopSynchroniserThread () {
  if (_synchroniserThread != nullptr) {
    LOG_TRACE("stopping wal synchroniser thread");

    _synchroniserThread->stop();
    _synchroniserThread->shutdown();

    delete _synchroniserThread;
    _synchroniserThread = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the allocator thread
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startAllocatorThread () {
  _allocatorThread = new AllocatorThread(this);

  if (_allocatorThread == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _allocatorThread->start()) {
    delete _allocatorThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the allocator thread
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stopAllocatorThread () {
  if (_allocatorThread != nullptr) {
    LOG_TRACE("stopping wal allocator thread");

    _allocatorThread->stop();
    _allocatorThread->shutdown();

    delete _allocatorThread;
    _allocatorThread = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the collector thread
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startCollectorThread () {
  _collectorThread = new CollectorThread(this);

  if (_collectorThread == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _collectorThread->start()) {
    delete _collectorThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the collector thread
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stopCollectorThread () {
  if (_collectorThread != nullptr) {
    LOG_TRACE("stopping wal collector thread");

    _collectorThread->stop();
    _collectorThread->shutdown();

    delete _collectorThread;
    _collectorThread = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check which logfiles are present in the log directory
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::inventory () {
  int res = ensureDirectory();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
    
  LOG_TRACE("scanning wal directory: '%s'", _directory.c_str());

  std::vector<std::string> files = basics::FileUtils::listFiles(_directory);

  for (auto it = files.begin(); it != files.end(); ++it) {
    regmatch_t matches[2];
    std::string const file = (*it);
    char const* s = file.c_str();

    if (regexec(&_regex, s, sizeof(matches) / sizeof(matches[1]), matches, 0) == 0) {
      Logfile::IdType const id = basics::StringUtils::uint64(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);

      if (id == 0) {
        LOG_WARNING("encountered invalid id for logfile '%s'. ids must be > 0", file.c_str());
      }
      else {
        // update global tick
        TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

        WRITE_LOCKER(_logfilesLock);
        _logfiles.insert(make_pair(id, nullptr));
      }
    }
  }
     
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scan the logfiles in the log directory
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::openLogfiles () {
  WRITE_LOCKER(_logfilesLock);

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile::IdType const id = (*it).first;
    std::string const filename = logfileName(id);

    assert((*it).second == nullptr);

    TRI_datafile_t* df = TRI_OpenDatafile(filename.c_str());

    if (df == nullptr) {
      int res = TRI_errno();

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_ERROR("unable to open logfile '%s': %s", filename.c_str(), TRI_errno_string(res));
        return res;
      }
    }
    
    // TODO: the last logfile is probably still a journal. must fix the "false" value
    (*it).second = new Logfile(df);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate a new datafile
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::allocateDatafile () {
  Logfile::IdType const id = nextId();
  std::string const filename = logfileName(id);

  TRI_datafile_t* df = TRI_CreateDatafile(filename.c_str(), id, static_cast<TRI_voc_size_t>(_configuration->filesize()));

  if (df == nullptr) {
    int res = TRI_errno();

    LOG_ERROR("unable to create datafile: %s", TRI_errno_string(res));
    return res;
  }
               
  WRITE_LOCKER(_logfilesLock);
  _logfiles.insert(make_pair(id, new Logfile(df)));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the recovery procedure
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::runRecovery () {
  // TODO
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an id for the next logfile
////////////////////////////////////////////////////////////////////////////////
        
Logfile::IdType LogfileManager::nextId () {
  return static_cast<Logfile::IdType>(TRI_NewTickServer());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure the wal logfiles directory is actually there
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::ensureDirectory () {
  if (! basics::FileUtils::isDirectory(_directory)) {
    int res;
    
    LOG_INFO("wal directory '%s' does not exist. creating it...", _directory.c_str());

    if (! basics::FileUtils::createDirectory(_directory, &res)) {
      LOG_ERROR("could not create wal directory: '%s': %s", _directory.c_str(), TRI_errno_string(res));
      return res;
    }
  }

  if (! basics::FileUtils::isDirectory(_directory)) {
    LOG_ERROR("wal directory '%s' does not exist", _directory.c_str());
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an absolute filename for a logfile id
////////////////////////////////////////////////////////////////////////////////

std::string LogfileManager::logfileName (Logfile::IdType id) const {
  return _directory + std::string("logfile-") + basics::StringUtils::itoa(id) + std::string(".db");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the absolute name of the shutdown file
////////////////////////////////////////////////////////////////////////////////

std::string LogfileManager::shutdownFilename () const {
  return _directory + std::string("SHUTDOWN");
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
