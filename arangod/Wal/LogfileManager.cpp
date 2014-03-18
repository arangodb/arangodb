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
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/JsonHelper.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Wal/AllocatorThread.h"
#include "Wal/CollectorThread.h"
#include "Wal/Configuration.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the logfile manager
////////////////////////////////////////////////////////////////////////////////

LogfileManager::LogfileManager (Configuration* configuration) 
  : _configuration(configuration),
    _tickLock(),
    _tick(0),
    _allocatorThread(nullptr),
    _collectorThread(nullptr),
    _logfileIdLock(),
    _logfileId(0),
    _logfilesLock(),
    _logfiles(),
    _directory(configuration->directory()) {
  
  int res = regcomp(&_regex, "^logfile-([0-9][0-9]*)\\.db$", REG_EXTENDED);

  if (res != 0) {
    THROW_INTERNAL_ERROR("could not compile regex"); 
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the logfile manager
////////////////////////////////////////////////////////////////////////////////

LogfileManager::~LogfileManager () {
  shutdown();
  regfree(&_regex);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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

  res = reserveSpace();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not reserve wal space: %s", TRI_errno_string(res));
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
  // stop threads
  stopCollectorThread();
  stopAllocatorThread();

  // close all open logfiles
  WRITE_LOCKER(_logfilesLock);

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr) {
      delete logfile;
    }
  }

  _logfiles.clear();

  int res = writeShutdownInfo();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not write wal shutdown info: %s", TRI_errno_string(res));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve space in a log file
////////////////////////////////////////////////////////////////////////////////

LogEntry LogfileManager::reserve (size_t size) {
  LogEntry::TickType const tick = nextTick();

  LogEntry entry = LogEntry(0, size, tick);

  return entry;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief increases the tick value and returns it
////////////////////////////////////////////////////////////////////////////////

LogEntry::TickType LogfileManager::nextTick () {
  WRITE_LOCKER(_tickLock);
  return ++_tick;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last assigned tick
////////////////////////////////////////////////////////////////////////////////

LogEntry::TickType LogfileManager::currentTick () {
  READ_LOCKER(_tickLock);
  return _tick;
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

  uint64_t const tick = basics::JsonHelper::stringUInt64(json, "maxTick");

  _tick = static_cast<LogEntry::TickType>(tick);
  
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  return TRI_ERROR_NO_ERROR; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the shutdown information
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::writeShutdownInfo () {
  std::string const filename = shutdownFilename();

  std::string content;
  content.append("{\"maxTick\":\"");
  content.append(basics::StringUtils::itoa(currentTick()));
  content.append("\"}");

  // TODO: spit() doesn't return success/failure. FIXME!
  basics::FileUtils::spit(filename, content);
  
  return TRI_ERROR_NO_ERROR; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the allocator thread
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startAllocatorThread () {
  _allocatorThread = new AllocatorThread(this);

  if (_allocatorThread == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _allocatorThread->init() ||
      ! _allocatorThread->start()) {
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

  if (! _collectorThread->init() ||
      ! _collectorThread->start()) {
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
    
  LOG_INFO("scanning wal directory: '%s'", _directory.c_str());
  std::vector<std::string> files = basics::FileUtils::listFiles(_directory);

  for (auto it = files.begin(); it != files.end(); ++it) {
    regmatch_t matches[2];
    std::string const file = (*it);
    char const* s = file.c_str();

    if (regexec(&_regex, s, sizeof(matches) / sizeof(matches[1]), matches, 0) == 0) {
      Logfile::IdType const id = basics::StringUtils::uint64(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);

      WRITE_LOCKER(_logfilesLock);
      _logfiles.insert(make_pair(id, nullptr));
    }
  }
     
  // now pick up the logfile with the highest id and use its id

  READ_LOCKER(_logfilesLock);
  if (! _logfiles.empty()) {
    auto it = _logfiles.rbegin();

    MUTEX_LOCKER(_logfileIdLock);
    _logfileId = (*it).first;
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
    
    (*it).second = new Logfile(df);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve space in the logfile directory
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::reserveSpace () {
  while (shouldAllocate()) {
    int res = allocateDatafile();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to allocate new datafile: %s", TRI_errno_string(res));
      
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a new datafile must be allocated
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::shouldAllocate () {
  uint64_t const maximum    = _configuration->maximumSize();
  uint64_t const reserve    = _configuration->reserveSize();
  uint64_t const allocated  = allocatedSize();
  uint64_t const free       = freeSize();

  if (allocated >= maximum) {
    return false;
  }
  
  if (free >= reserve) {
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of bytes allocated in datafiles
////////////////////////////////////////////////////////////////////////////////

uint64_t LogfileManager::allocatedSize () {
  uint64_t allocated = 0;

  READ_LOCKER(_logfilesLock);
  
  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;
    
    assert(logfile != nullptr);
    allocated += logfile->allocatedSize();
  }

  return allocated;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of free bytes in already allocated datafiles
////////////////////////////////////////////////////////////////////////////////

uint64_t LogfileManager::freeSize () {
  uint64_t free = 0;

  READ_LOCKER(_logfilesLock);
  
  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    assert(logfile != nullptr);
    free += logfile->freeSize();
  }

  return free;
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
  MUTEX_LOCKER(_logfileIdLock);
  return ++_logfileId;
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
