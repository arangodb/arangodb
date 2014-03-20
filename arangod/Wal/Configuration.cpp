////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log configuration
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

#include "Configuration.h"
#include "BasicsC/logging.h"
#include "Wal/LogfileManager.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the configuration
////////////////////////////////////////////////////////////////////////////////

Configuration::Configuration () 
  : ApplicationFeature("wal"),
    _logfileManager(nullptr),
    _filesize(32 * 1024 * 1024),
    _numberOfLogfiles(4),
    _reserveSize(16 * 1024 * 1024),
    _directory() {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the configuration
////////////////////////////////////////////////////////////////////////////////

Configuration::~Configuration () {
  if (_logfileManager != nullptr) {
    delete _logfileManager;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void Configuration::setupOptions (std::map<std::string, triagens::basics::ProgramOptionsDescription>& options) {
  options["Write-ahead log options:help-wal"]
    ("wal.filesize", &_filesize, "size of each logfile")
    ("wal.logfiles", &_numberOfLogfiles, "target number of logfiles")
    ("wal.reserve", &_reserveSize, "minimum space to reserve for new data")
    ("wal.directory", &_directory, "logfile directory")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool Configuration::prepare () {
  if (_directory.empty()) {
    LOG_FATAL_AND_EXIT("no directory specified for write-ahead logs. Please provide the --wal.directory option");
  }

  if (_directory[_directory.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
    // append a trailing slash to directory name
    _directory.push_back(TRI_DIR_SEPARATOR_CHAR);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool Configuration::start () {
  _logfileManager = new LogfileManager(this);
 
  assert(_logfileManager != nullptr);

  int res = _logfileManager->startup();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not initialise wal components: %s", TRI_errno_string(res));
    return false;
  }

  return true;
}
  
////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool Configuration::open () {
  for (size_t i = 0; i < 64 * 1024 * 1024; ++i) {
    void* p = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, 64, true);
    TRI_df_marker_t* marker = static_cast<TRI_df_marker_t*>(p);

    marker->_type = TRI_DF_MARKER_HEADER;
    marker->_size = 64;
    marker->_crc  = 0;
    marker->_tick = 0;

if (i % 500000 == 0) {
  LOG_INFO("now at: %d", (int) i);
}
    memcpy(static_cast<char*>(p) + sizeof(TRI_df_marker_t), "the fox is brown\0", strlen("the fox is brown") + 1);
    _logfileManager->allocateAndWrite(p, static_cast<uint32_t>(64), (int) i);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, p);
  }

  LOG_INFO("done");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void Configuration::close () {
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void Configuration::stop () {
  if (_logfileManager != nullptr) {
    _logfileManager->shutdown();
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
