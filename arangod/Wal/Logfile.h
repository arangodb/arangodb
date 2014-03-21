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

#ifndef TRIAGENS_WAL_LOGFILE_H
#define TRIAGENS_WAL_LOGFILE_H 1

#include "Basics/Common.h"
#include "BasicsC/logging.h"
#include "VocBase/datafile.h"

namespace triagens {
  namespace wal {

// -----------------------------------------------------------------------------
// --SECTION--                                                     class Logfile
// -----------------------------------------------------------------------------

    class Logfile {

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for logfile ids
////////////////////////////////////////////////////////////////////////////////

        typedef TRI_voc_fid_t IdType;

////////////////////////////////////////////////////////////////////////////////
/// @brief logfile collection status
////////////////////////////////////////////////////////////////////////////////

        enum class CollectionStatusType {
          UNCOLLECTED,
          REQUESTED,
          DONE
        };

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief Logfile
////////////////////////////////////////////////////////////////////////////////

        Logfile (Logfile const&);
        Logfile& operator= (Logfile const&);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create a logfile
////////////////////////////////////////////////////////////////////////////////

        Logfile (TRI_datafile_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a logfile
////////////////////////////////////////////////////////////////////////////////

        ~Logfile ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the datafile pointer
////////////////////////////////////////////////////////////////////////////////

      TRI_datafile_t* df () {
        return _df;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the logfile id
////////////////////////////////////////////////////////////////////////////////

      Logfile::IdType id () const {
        return _df->_fid;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the allocated size of the logfile
////////////////////////////////////////////////////////////////////////////////

      uint64_t allocatedSize () const {
        return static_cast<uint64_t>(_df->_maximalSize);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the size of the free space in the logfile
////////////////////////////////////////////////////////////////////////////////

      uint64_t freeSize () const {
        if (_df->_isSealed) {
          return 0;
        }

        return static_cast<uint64_t>(allocatedSize() - _df->_footerSize - _df->_currentSize);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker of the specified size can be written into
/// the logfile
////////////////////////////////////////////////////////////////////////////////

      bool isWriteable (uint32_t size) const {
        if (isSealed()) {
          return false;
        }
        if (freeSize() < static_cast<uint64_t>(size)) {
          return false;
        }

        return true;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the logfile is sealed
////////////////////////////////////////////////////////////////////////////////

      bool isSealed () const {
        // TODO: introduce a logfile status instead of this boolean
        return _sealRequested;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the logfile can be collected
////////////////////////////////////////////////////////////////////////////////

      bool canCollect () const {
        return isSealed() && (_collectionStatus == CollectionStatusType::UNCOLLECTED);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the logfile to status requested
////////////////////////////////////////////////////////////////////////////////

      void setCollectionRequested () {
        LOG_INFO("requesting collection for logfile %llu", (unsigned long long) id());
        _collectionStatus = CollectionStatusType::REQUESTED;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the logfile to status collected
////////////////////////////////////////////////////////////////////////////////

      void setCollectionDone () {
        LOG_INFO("collection done for logfile %llu", (unsigned long long) id());
        _collectionStatus = CollectionStatusType::DONE;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief seal a logfile
////////////////////////////////////////////////////////////////////////////////

      void seal ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the datafile entry
////////////////////////////////////////////////////////////////////////////////

      TRI_datafile_t* _df;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a seal was requested
////////////////////////////////////////////////////////////////////////////////

      bool _sealRequested;

////////////////////////////////////////////////////////////////////////////////
/// @brief logfile collection status
////////////////////////////////////////////////////////////////////////////////
      
      CollectionStatusType _collectionStatus;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
