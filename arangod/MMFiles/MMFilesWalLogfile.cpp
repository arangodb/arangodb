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

#include "MMFilesWalLogfile.h"
#include "Basics/FileUtils.h"
#include "Basics/encoding.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "MMFiles/MMFilesDatafileHelper.h"

using namespace arangodb;

/// @brief create the logfile
MMFilesWalLogfile::MMFilesWalLogfile(MMFilesWalLogfile::IdType id,
                                     MMFilesDatafile* df, StatusType status)
    : _id(id), _users(0), _df(df), _status(status), _collectQueueSize(0) {}

/// @brief destroy the logfile
MMFilesWalLogfile::~MMFilesWalLogfile() { delete _df; }

/// @brief create a new logfile
MMFilesWalLogfile* MMFilesWalLogfile::createNew(std::string const& filename,
                                                MMFilesWalLogfile::IdType id,
                                                uint32_t size) {
  std::unique_ptr<MMFilesDatafile> df(
      MMFilesDatafile::create(filename, id, static_cast<uint32_t>(size), false));

  if (df == nullptr) {
    int res = TRI_errno();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("67643", ERR, arangodb::Logger::ENGINES) << "unable to create logfile '" << filename
                                                << "': " << TRI_errno_string(res);
      return nullptr;
    }
  }

  MMFilesWalLogfile* logfile = new MMFilesWalLogfile(id, df.get(), StatusType::EMPTY);
  df.release();
  return logfile;
}

/// @brief open an existing logfile
MMFilesWalLogfile* MMFilesWalLogfile::openExisting(std::string const& filename,
                                                   MMFilesWalLogfile::IdType id,
                                                   bool wasCollected, bool ignoreErrors) {
  std::unique_ptr<MMFilesDatafile> df(MMFilesDatafile::open(filename, ignoreErrors, false));

  if (df == nullptr) {
    int res = TRI_errno();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("6be8a", ERR, arangodb::Logger::ENGINES) << "unable to open logfile '" << filename
                                                << "': " << TRI_errno_string(res);
    } else {
      // cannot figure out the type of error
      LOG_TOPIC("44b1f", ERR, arangodb::Logger::ENGINES)
          << "unable to open logfile '" << filename << "'";
    }
    return nullptr;
  }

  StatusType status = StatusType::OPEN;

  if (df->isSealed()) {
    status = StatusType::SEALED;
  }

  if (wasCollected) {
    // the logfile was already collected
    status = StatusType::COLLECTED;
  }

  MMFilesWalLogfile* logfile = new MMFilesWalLogfile(id, df.get(), status);
  df.release();
  return logfile;
}

/// @brief reserve space and update the current write position
char* MMFilesWalLogfile::reserve(size_t size) {
  return _df->advanceWritePosition(encoding::alignedSize<size_t>(size));
}
