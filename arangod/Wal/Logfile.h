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
#include "VocBase/marker.h"

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
/// @brief logfile status
////////////////////////////////////////////////////////////////////////////////

        enum class StatusType : uint32_t {
          UNKNOWN,
          EMPTY,
          OPEN,
          SEAL_REQUESTED,
          SEALED,
          COLLECTION_REQUESTED,
          COLLECTED
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

        Logfile (Logfile::IdType,
                 TRI_datafile_t*,
                 StatusType);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a logfile
////////////////////////////////////////////////////////////////////////////////

        ~Logfile ();

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new logfile
////////////////////////////////////////////////////////////////////////////////

      static Logfile* createNew (std::string const&,
                                 Logfile::IdType,
                                 uint32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief open an existing logfile
////////////////////////////////////////////////////////////////////////////////

      static Logfile* openExisting (std::string const&,
                                    Logfile::IdType,
                                    bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a logfile is empty
////////////////////////////////////////////////////////////////////////////////

      static int judge (std::string const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the datafile pointer
////////////////////////////////////////////////////////////////////////////////

      inline TRI_datafile_t* df () const {
        return _df;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the file descriptor
////////////////////////////////////////////////////////////////////////////////

      inline int fd () const {
        return _df->_fd;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the logfile id
////////////////////////////////////////////////////////////////////////////////

      inline Logfile::IdType id () const {
        return _id;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the logfile status
////////////////////////////////////////////////////////////////////////////////

      inline Logfile::StatusType status () const {
        return _status;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the allocated size of the logfile
////////////////////////////////////////////////////////////////////////////////

      inline uint64_t allocatedSize () const {
        return static_cast<uint64_t>(_df->_maximalSize);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the size of the free space in the logfile
////////////////////////////////////////////////////////////////////////////////

      uint64_t freeSize () const {
        if (isSealed()) {
          return 0;
        }

        return static_cast<uint64_t>(allocatedSize() - _df->_currentSize - overhead());
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

      inline bool isSealed () const {
        return (_status == StatusType::SEAL_REQUESTED ||
                _status == StatusType::SEALED ||
                _status == StatusType::COLLECTION_REQUESTED ||
                _status == StatusType::COLLECTED);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the logfile can be sealed
////////////////////////////////////////////////////////////////////////////////

      inline bool canBeSealed () const {
        return (_status == StatusType::SEAL_REQUESTED);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the logfile can be collected
////////////////////////////////////////////////////////////////////////////////

      inline bool canBeCollected () const {
        return (_status == StatusType::SEALED);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the logfile can be removed
////////////////////////////////////////////////////////////////////////////////

      inline bool canBeRemoved () const {
        return (_status == StatusType::COLLECTED);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the logfile overhead
////////////////////////////////////////////////////////////////////////////////

      static inline uint32_t overhead () {
        return TRI_JOURNAL_OVERHEAD;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the logfile status as a string
////////////////////////////////////////////////////////////////////////////////

      static std::string statusText (StatusType status) {
        switch (status) {
          case StatusType::EMPTY:
            return "empty";
          case StatusType::OPEN:
            return "open";
          case StatusType::SEAL_REQUESTED:
            return "seal-requested";
          case StatusType::SEALED:
            return "sealed";
          case StatusType::COLLECTION_REQUESTED:
            return "collection-requested";
          case StatusType::COLLECTED:
            return "collected";
          case StatusType::UNKNOWN:
          default:
            return "unknown";
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief change the logfile status
////////////////////////////////////////////////////////////////////////////////

      void setStatus (StatusType status) {
        switch (status) {
          case StatusType::UNKNOWN:
          case StatusType::EMPTY:
            assert(false);
            break;

          case StatusType::OPEN:
            assert(_status == StatusType::EMPTY);
            break;

          case StatusType::SEAL_REQUESTED:
            assert(_status == StatusType::OPEN);
            break;

          case StatusType::SEALED:
            assert(_status == StatusType::SEAL_REQUESTED);
            break;

          case StatusType::COLLECTION_REQUESTED:
            assert(_status == StatusType::SEALED);
            break;

          case StatusType::COLLECTED:
            assert(_status == StatusType::COLLECTION_REQUESTED);
            break;
        }

        LOG_TRACE("changing logfile status from %s to %s for logfile %llu", 
                  statusText(_status).c_str(), 
                  statusText(status).c_str(),
                  (unsigned long long) id());
        _status = status;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve space and update the current write position
////////////////////////////////////////////////////////////////////////////////

      char* reserve (size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a header marker
////////////////////////////////////////////////////////////////////////////////

      TRI_df_header_marker_t getHeaderMarker () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a footer marker
////////////////////////////////////////////////////////////////////////////////

      TRI_df_footer_marker_t getFooterMarker () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the logfile id
////////////////////////////////////////////////////////////////////////////////

      Logfile::IdType const _id;

////////////////////////////////////////////////////////////////////////////////
/// @brief the datafile entry
////////////////////////////////////////////////////////////////////////////////

      TRI_datafile_t* _df;

////////////////////////////////////////////////////////////////////////////////
/// @brief logfile status
////////////////////////////////////////////////////////////////////////////////
      
      StatusType _status;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
