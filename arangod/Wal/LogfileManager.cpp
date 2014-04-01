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
#include "Wal/Slots.h"
#include "Wal/SynchroniserThread.h"
#include "Wal/TestThread.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                              class LogfileManager
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum logfile size
////////////////////////////////////////////////////////////////////////////////

const uint32_t LogfileManager::MinFilesize = 8 * 1024 * 1024;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the logfile manager
////////////////////////////////////////////////////////////////////////////////

LogfileManager::LogfileManager (std::string* databasePath)
  : ApplicationFeature("logfile-manager"),
    _databasePath(databasePath),
    _directory(),
    _filesize(32 * 1024 * 1024),
    _reserveLogfiles(4),
    _historicLogfiles(10),
    _maxOpenLogfiles(10),
    _allowOversizeEntries(true),
    _slots(nullptr),
    _synchroniserThread(nullptr),
    _allocatorThread(nullptr),
    _collectorThread(nullptr),
    _logfilesLock(),
    _lastCollectedId(0),
    _logfiles(),
    _regex(),
    _shutdown(0) {
  
  LOG_TRACE("creating wal logfile manager");

  int res = regcomp(&_regex, "^logfile-([0-9][0-9]*)\\.db$", REG_EXTENDED);

  if (res != 0) {
    THROW_INTERNAL_ERROR("could not compile regex"); 
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the logfile manager
////////////////////////////////////////////////////////////////////////////////

LogfileManager::~LogfileManager () {
  LOG_TRACE("shutting down wal logfile manager");

  regfree(&_regex);

  if (_slots != nullptr) {
    delete _slots;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setupOptions (std::map<std::string, triagens::basics::ProgramOptionsDescription>& options) {
  options["Write-ahead log options:help-wal"]
    ("wal.allow-oversize-entries", &_allowOversizeEntries, "allow entries that are bigger than --wal.logfile-size")
    ("wal.logfile-size", &_filesize, "size of each logfile")
    ("wal.historic-logfiles", &_historicLogfiles, "maximum number of historic logfiles to keep after collection")
    ("wal.reserve-logfiles", &_reserveLogfiles, "maximum number of reserve logfiles to maintain")
    ("wal.open-logfiles", &_maxOpenLogfiles, "maximum number of parallel open logfiles")
    ("wal.directory", &_directory, "logfile directory")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::prepare () {
  if (_directory.empty()) {
    // use global configuration variable
    _directory = *_databasePath;

    // append "/journals"
    if (_directory[_directory.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
      // append a trailing slash to directory name
      _directory.push_back(TRI_DIR_SEPARATOR_CHAR);
    }
    _directory.append("journals");
  }
    
  if (_directory.empty()) {
    LOG_FATAL_AND_EXIT("no directory specified for write-ahead logs. Please use the --wal.directory option");
  }

  if (_directory[_directory.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
    // append a trailing slash to directory name
    _directory.push_back(TRI_DIR_SEPARATOR_CHAR);
  }

  if (_filesize < MinFilesize) {
    // minimum filesize per logfile
    LOG_FATAL_AND_EXIT("invalid logfile size. Please use a value of at least %lu", (unsigned long) MinFilesize);
  }
  
  _filesize = ((_filesize + PageSize - 1) / PageSize) * PageSize;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::start () {
  _slots = new Slots(this, 1048576, 0);

  int res = inventory();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not create wal logfile inventory: %s", TRI_errno_string(res));
    return false;
  }
 
  std::string const shutdownFile = shutdownFilename();
  bool const shutdownFileExists = basics::FileUtils::exists(shutdownFile); 

  if (shutdownFileExists) {
    res = readShutdownInfo();
  
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("could not open shutdown file '%s': %s", 
                shutdownFile.c_str(), 
                TRI_errno_string(res));
      return false;
    }

    LOG_TRACE("logfile manager last tick: %llu, last collected: %llu", 
              (unsigned long long) _slots->lastAssignedTick(),
              (unsigned long long) _lastCollectedId);
  }

  res = openLogfiles(shutdownFileExists);
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not open wal logfiles: %s", 
              TRI_errno_string(res));
    return false;
  }

  res = startSynchroniserThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start wal synchroniser thread: %s", TRI_errno_string(res));
    return false;
  }

  res = startAllocatorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start wal allocator thread: %s", TRI_errno_string(res));
    return false;
  }
  
  res = startCollectorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start wal collector thread: %s", TRI_errno_string(res));
    return false;
  }

  if (shutdownFileExists) {
    // delete the shutdown file if it existed
    if (! basics::FileUtils::remove(shutdownFile, &res)) {
      LOG_ERROR("could not remove shutdown file '%s': %s", shutdownFile.c_str(), TRI_errno_string(res));
      return false;
    }
  }

  LOG_TRACE("wal logfile manager configuration: historic logfiles: %lu, reserve logfiles: %lu, filesize: %lu",
            (unsigned long) _historicLogfiles,
            (unsigned long) _reserveLogfiles,
            (unsigned long) _filesize);

  return true;
}
  
////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::open () {
  TestThread* threads[4];
  for (size_t i = 0; i < 4; ++i) {
    threads[i] = new TestThread(this);
    threads[i]->start();
  }

  LOG_INFO("sleeping");
  sleep(60);

  for (size_t i = 0; i < 4; ++i) {
    threads[i]->stop();
    delete threads[i];
  }

  LOG_INFO("done");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::close () {
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stop () {
  if (_shutdown > 0) {
    return;
  }

  _shutdown = 1;

  // stop threads
  
  LOG_TRACE("stopping collector thread");
  stopCollectorThread();
  
  LOG_TRACE("stopping allocator thread");
  stopAllocatorThread();
  
  LOG_TRACE("stopping synchroniser thread");
  stopSynchroniserThread();
  
  // close all open logfiles
  LOG_TRACE("closing logfiles");
  closeLogfiles();

  int res = writeShutdownInfo();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not write wal shutdown info: %s", TRI_errno_string(res));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not it is currently allowed to create an additional 
/// logfile
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::logfileCreationAllowed (uint32_t size) {
  if (size + Logfile::overhead() > filesize()) {
    // oversize entry. this is always allowed because otherwise everything would
    // lock
    return true;
  }

  uint32_t numberOfLogfiles = 0;

  // note: this information could also be cached instead of being recalculated
  // everytime
  READ_LOCKER(_logfilesLock);
     
  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;
  
    assert(logfile != nullptr);

    if (logfile->status() == Logfile::StatusType::OPEN ||
        logfile->status() == Logfile::StatusType::SEAL_REQUESTED) { 
      ++numberOfLogfiles;
    }
  }

  return (numberOfLogfiles <= _maxOpenLogfiles);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not there are reserve logfiles
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::hasReserveLogfiles () {
  uint32_t numberOfLogfiles = 0;

  // note: this information could also be cached instead of being recalculated
  // everytime
  READ_LOCKER(_logfilesLock);
   
  // reverse-scan the logfiles map  
  for (auto it = _logfiles.rbegin(); it != _logfiles.rend(); ++it) {
    Logfile* logfile = (*it).second;
  
    assert(logfile != nullptr);

    if (logfile->freeSize() > 0 && ! logfile->isSealed()) {
      if (++numberOfLogfiles >= reserveLogfiles()) {
        return true;
      }
    }
  }

  return false;
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

SlotInfo LogfileManager::allocate (uint32_t size) {
  if (size > maxEntrySize()) {
    // entry is too big
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  if (size > _filesize && ! _allowOversizeEntries) {
    // entry is too big for a logfile
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  return _slots->nextUnused(size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finalise a log entry
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::finalise (SlotInfo& slotInfo,
                               bool waitForSync) {
  _slots->returnUsed(slotInfo, waitForSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data into the logfile
/// this is a convenience function that combines allocate, memcpy and finalise
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::allocateAndWrite (void* src,
                                      uint32_t size,
                                      bool waitForSync) {

  SlotInfo slotInfo = allocate(size);
 
  if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
    return slotInfo.errorCode;
  }

  assert(slotInfo.slot != nullptr);

  slotInfo.slot->fill(src, size);

  finalise(slotInfo, waitForSync);
  return slotInfo.errorCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief re-inserts a logfile back into the inventory only
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::relinkLogfile (Logfile* logfile) {
  Logfile::IdType const id = logfile->id();

  WRITE_LOCKER(_logfilesLock);
  _logfiles.insert(make_pair(id, logfile));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a logfile from the inventory only
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::unlinkLogfile (Logfile* logfile) {
  Logfile::IdType const id = logfile->id();

  WRITE_LOCKER(_logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return false;
  }

  _logfiles.erase(it);
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a logfile from the inventory only
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::unlinkLogfile (Logfile::IdType id) {
  WRITE_LOCKER(_logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return nullptr;
  }

  _logfiles.erase(it);
  
  return (*it).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a logfile from the inventory and in the file system
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::removeLogfile (Logfile* logfile,
                                    bool unlink) {
  if (unlink) {
    unlinkLogfile(logfile);
  }

  // old filename
  Logfile::IdType const id = logfile->id();
  std::string const filename = logfileName(id);
  
  LOG_TRACE("removing logfile '%s'", filename.c_str());

  // now close the logfile
  delete logfile;
  
  int res = TRI_ERROR_NO_ERROR;
  // now physically remove the file

  if (! basics::FileUtils::remove(filename, &res)) {
    LOG_ERROR("unable to remove logfile '%s': %s", 
              filename.c_str(), 
              TRI_errno_string(res));
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the status of a logfile to open
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setLogfileOpen (Logfile* logfile) {
  assert(logfile != nullptr);

  WRITE_LOCKER(_logfilesLock);
  logfile->setStatus(Logfile::StatusType::OPEN);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the status of a logfile to seal-requested
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setLogfileSealRequested (Logfile* logfile) {
  assert(logfile != nullptr);

  WRITE_LOCKER(_logfilesLock);
  logfile->setStatus(Logfile::StatusType::SEAL_REQUESTED);
  signalSync();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the status of a logfile to sealed
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setLogfileSealed (Logfile* logfile) {
  assert(logfile != nullptr);

  WRITE_LOCKER(_logfilesLock);
  logfile->setStatus(Logfile::StatusType::SEALED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the status of a logfile to sealed
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setLogfileSealed (Logfile::IdType id) {
  WRITE_LOCKER(_logfilesLock);

  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return;
  }

  (*it).second->setStatus(Logfile::StatusType::SEALED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of a logfile
////////////////////////////////////////////////////////////////////////////////

Logfile::StatusType LogfileManager::getLogfileStatus (Logfile::IdType id) {
  READ_LOCKER(_logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return Logfile::StatusType::UNKNOWN;
  }
  return (*it).second->status();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the file descriptor of a logfile
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::getLogfileDescriptor (Logfile::IdType id) {
  READ_LOCKER(_logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    // error
    LOG_ERROR("could not find logfile %llu", (unsigned long long) id);
    return -1;
  }

  Logfile* logfile = (*it).second;
  assert(logfile != nullptr);

  return logfile->fd();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile for writing. this may return nullptr
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getWriteableLogfile (uint32_t size,
                                              Logfile::StatusType& status) {
  size_t iterations = 0;

  while (++iterations < 1000) {
    {
      WRITE_LOCKER(_logfilesLock);
      auto it = _logfiles.begin();

      while (it != _logfiles.end()) {
        Logfile* logfile = (*it).second;

        assert(logfile != nullptr);
        
        if (logfile->isWriteable(size)) {
          // found a logfile, update the status variable and return the logfile
          status = logfile->status();
          return logfile;
        }

        if (logfile->status() == Logfile::StatusType::EMPTY && 
            ! logfile->isWriteable(size)) {
          // we found an empty logfile, but the entry won't fit

          // delete the logfile from the sequence of logfiles
          _logfiles.erase(it++);

          // and physically remove the file
          // note: this will also delete the logfile object!
          removeLogfile(logfile, false);

        }
        else {
          ++it;
        }
      }
    }

    // signal & sleep outside the lock
    _allocatorThread->signal(size);
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

    if (logfile != nullptr && logfile->canBeCollected()) {
      return logfile;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile to remove. this may return nullptr
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getRemovableLogfile () {
  uint32_t numberOfLogfiles = 0;
  Logfile* first = nullptr;

  READ_LOCKER(_logfilesLock);

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr && logfile->canBeRemoved()) {
      if (first == nullptr) {
        first = logfile;
      }

      if (++numberOfLogfiles > historicLogfiles()) { 
        assert(first != nullptr);
        return first;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a file as being requested for collection
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setCollectionRequested (Logfile* logfile) {
  assert(logfile != nullptr);

  {
    WRITE_LOCKER(_logfilesLock);
    logfile->setStatus(Logfile::StatusType::COLLECTION_REQUESTED);
  }
  
  _collectorThread->signal();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a file as being done with collection
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setCollectionDone (Logfile* logfile) {
  assert(logfile != nullptr);

  {
    WRITE_LOCKER(_logfilesLock);
    logfile->setStatus(Logfile::StatusType::COLLECTED);
  }

  _collectorThread->signal();
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
  _slots->setLastAssignedTick(static_cast<Slot::TickType>(lastTick));
  
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
  content.append(basics::StringUtils::itoa(_slots->lastAssignedTick()));
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

int LogfileManager::openLogfiles (bool wasCleanShutdown) {
  WRITE_LOCKER(_logfilesLock);

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ) {
    Logfile::IdType const id = (*it).first;
    std::string const filename = logfileName(id);

    assert((*it).second == nullptr);

    int res = Logfile::judge(filename);

    if (res == TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
      if (basics::FileUtils::remove(filename, 0)) {
        LOG_INFO("removing empty wal logfile '%s'", filename.c_str());
      }
      _logfiles.erase(it++);
      continue;
    }

    Logfile* logfile = Logfile::openExisting(filename, id, id <= _lastCollectedId);

    if (logfile == nullptr) {
      _logfiles.erase(it++);
      continue;
    }
      
    (*it).second = logfile;
    ++it;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocates a new reserve logfile
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::createReserveLogfile (uint32_t size) {
  Logfile::IdType const id = nextId();
  std::string const filename = logfileName(id);

  LOG_TRACE("creating empty logfile '%s' with size %lu", 
            filename.c_str(), 
            (unsigned long) size);

  uint32_t realsize;
  if (size > 0 && size > filesize()) {
    // create a logfile with the requested size
    realsize = size + Logfile::overhead();
  }
  else {
    // create a logfile with default size
    realsize = filesize();
  }
    
  Logfile* logfile = Logfile::createNew(filename.c_str(), id, realsize);

  if (logfile == nullptr) {
    int res = TRI_errno();

    LOG_ERROR("unable to create logfile: %s", TRI_errno_string(res));
    return res;
  }
               
  WRITE_LOCKER(_logfilesLock);
  _logfiles.insert(make_pair(id, logfile));

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
/// @brief return the absolute name of the shutdown file
////////////////////////////////////////////////////////////////////////////////

std::string LogfileManager::shutdownFilename () const {
  return _directory + std::string("SHUTDOWN");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an absolute filename for a logfile id
////////////////////////////////////////////////////////////////////////////////

std::string LogfileManager::logfileName (Logfile::IdType id) const {
  return _directory + std::string("logfile-") + basics::StringUtils::itoa(id) + std::string(".db");
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
