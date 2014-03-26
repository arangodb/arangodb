////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log logfile
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

#include "Logfile.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the logfile
////////////////////////////////////////////////////////////////////////////////

Logfile::Logfile (Logfile::IdType id, 
                  TRI_datafile_t* df,
                  StatusType status)
  : _id(id),
    _df(df),
    _status(status) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the logfile
////////////////////////////////////////////////////////////////////////////////

Logfile::~Logfile () {
  if (_df != nullptr) {
    TRI_CloseDatafile(_df);
    TRI_FreeDatafile(_df);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new logfile
////////////////////////////////////////////////////////////////////////////////

Logfile* Logfile::create (std::string const& filename,
                          Logfile::IdType id,
                          uint32_t size) {
  TRI_datafile_t* df = TRI_CreateDatafile(filename.c_str(), id, static_cast<TRI_voc_size_t>(size), true);

  if (df == nullptr) {
    int res = TRI_errno();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to create logfile '%s': %s", 
                filename.c_str(), 
                TRI_errno_string(res));
      return nullptr;
    }
  }

  Logfile* logfile = new Logfile(id, df, StatusType::OPEN);
  return logfile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open an existing logfile
////////////////////////////////////////////////////////////////////////////////

Logfile* Logfile::open (std::string const& filename,
                        Logfile::IdType id) {
  TRI_datafile_t* df = TRI_OpenDatafile(filename.c_str());

  if (df == nullptr) {
    int res = TRI_errno();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to open logfile '%s': %s", 
                filename.c_str(), 
                TRI_errno_string(res));
      return nullptr;
    }
  }

  StatusType status = StatusType::OPEN;

  if (df->_isSealed) {
    status = StatusType::SEALED;
  }

  Logfile* logfile = new Logfile(id, df, status);
  return logfile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief seals a logfile
////////////////////////////////////////////////////////////////////////////////

int Logfile::seal () {
  // we're not really sealing the logfile as this isn't required
  int res = TRI_ERROR_NO_ERROR; // TRI_SealDatafile(_df);
  
  _df->_isSealed = true;
  _df->_state = TRI_DF_STATE_READ;

  if (res == TRI_ERROR_NO_ERROR) {
    LOG_INFO("sealed logfile %llu", (unsigned long long) id());

    setStatus(StatusType::SEALED);
  }

  return res;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
