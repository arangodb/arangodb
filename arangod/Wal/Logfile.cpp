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

#include "Logfile.h"
#include "Basics/FileUtils.h"
#include "Basics/files.h"

using namespace arangodb::wal;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the logfile
////////////////////////////////////////////////////////////////////////////////

Logfile::Logfile(Logfile::IdType id, TRI_datafile_t* df, StatusType status)
    : _id(id), _users(0), _df(df), _status(status), _collectQueueSize(0) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the logfile
////////////////////////////////////////////////////////////////////////////////

Logfile::~Logfile() {
  if (_df != nullptr) {
    TRI_CloseDatafile(_df);
    TRI_FreeDatafile(_df);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new logfile
////////////////////////////////////////////////////////////////////////////////

Logfile* Logfile::createNew(std::string const& filename, Logfile::IdType id,
                            uint32_t size) {
  TRI_datafile_t* df = TRI_CreateDatafile(
      filename.c_str(), id, static_cast<TRI_voc_size_t>(size), false);

  if (df == nullptr) {
    int res = TRI_errno();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "unable to create logfile '" << filename.c_str() << "': " << TRI_errno_string(res);
      return nullptr;
    }
  }

  return new Logfile(id, df, StatusType::EMPTY);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open an existing logfile
////////////////////////////////////////////////////////////////////////////////

Logfile* Logfile::openExisting(std::string const& filename, Logfile::IdType id,
                               bool wasCollected, bool ignoreErrors) {
  TRI_datafile_t* df = TRI_OpenDatafile(filename.c_str(), ignoreErrors);

  if (df == nullptr) {
    int res = TRI_errno();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "unable to open logfile '" << filename.c_str() << "': " << TRI_errno_string(res);
      return nullptr;
    }

    // cannot figure out the type of error
    LOG(ERR) << "unable to open logfile '" << filename.c_str() << "'";
    return nullptr;
  }

  StatusType status = StatusType::OPEN;

  if (df->_isSealed) {
    status = StatusType::SEALED;
  }

  if (wasCollected) {
    // the logfile was already collected
    status = StatusType::COLLECTED;
  }

  Logfile* logfile = new Logfile(id, df, status);
  return logfile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a logfile is empty
////////////////////////////////////////////////////////////////////////////////

int Logfile::judge(std::string const& filename) {
  off_t filesize = basics::FileUtils::size(filename);

  if (filesize == 0) {
    // empty logfile
    return TRI_ERROR_ARANGO_DATAFILE_EMPTY;
  }

  if (filesize < static_cast<off_t>(256 * sizeof(uint64_t))) {
    // too small
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  int fd = TRI_OPEN(filename.c_str(), O_RDWR | TRI_O_CLOEXEC);

  if (fd < 0) {
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  uint64_t buffer[256];

  if (!TRI_ReadPointer(fd, &buffer, 256 * sizeof(uint64_t))) {
    TRI_CLOSE(fd);
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  uint64_t* ptr = buffer;
  uint64_t* end = buffer + 256;

  while (ptr < end) {
    if (*ptr != 0) {
      TRI_CLOSE(fd);
      return TRI_ERROR_NO_ERROR;
    }
    ++ptr;
  }

  TRI_CLOSE(fd);
  return TRI_ERROR_ARANGO_DATAFILE_EMPTY;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve space and update the current write position
////////////////////////////////////////////////////////////////////////////////

char* Logfile::reserve(size_t size) {
  size = TRI_DF_ALIGN_BLOCK(size);

  char* result = _df->_next;

  _df->_next += size;
  _df->_currentSize += (TRI_voc_size_t)size;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a header marker
////////////////////////////////////////////////////////////////////////////////

TRI_df_header_marker_t Logfile::getHeaderMarker() const {
  TRI_df_header_marker_t header;
  size_t const size = sizeof(TRI_df_header_marker_t);
  TRI_InitMarkerDatafile((char*)&header, TRI_DF_MARKER_HEADER, size);

  header._version = TRI_DF_VERSION;
  header._maximalSize = static_cast<TRI_voc_size_t>(allocatedSize());
  header._fid = static_cast<TRI_voc_fid_t>(_id);

  return header;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a footer marker
////////////////////////////////////////////////////////////////////////////////

TRI_df_footer_marker_t Logfile::getFooterMarker() const {
  TRI_df_footer_marker_t footer;
  size_t const size = sizeof(TRI_df_footer_marker_t);
  TRI_InitMarkerDatafile((char*)&footer, TRI_DF_MARKER_FOOTER, size);

  return footer;
}
