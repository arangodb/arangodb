////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log logfile
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_WAL_LOGFILE_H
#define ARANGODB_WAL_LOGFILE_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/logging.h"
#include "VocBase/voc-types.h"
#include "VocBase/datafile.h"
#include "ShapedJson/shaped-json.h"
#include "Wal/Marker.h"

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
          UNKNOWN                = 0,
          EMPTY                  = 1,
          OPEN                   = 2,
          SEAL_REQUESTED         = 3,
          SEALED                 = 4,
          COLLECTION_REQUESTED   = 5,
          COLLECTED              = 6
        };

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief Logfile
////////////////////////////////////////////////////////////////////////////////

        Logfile (Logfile const&) = delete;
        Logfile& operator= (Logfile const&) = delete;

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
                                      bool,
                                      bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a logfile is empty
////////////////////////////////////////////////////////////////////////////////

        static int judge (std::string const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the filename
////////////////////////////////////////////////////////////////////////////////

        inline std::string filename () const {
          if (_df == nullptr) {
            return "";
          }
          return _df->getName(_df);
        }

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
/// @brief update the logfile tick status
////////////////////////////////////////////////////////////////////////////////

        inline void update (TRI_df_marker_t const* marker) {
          TRI_UpdateTicksDatafile(df(), marker);
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
          if (isSealed() || freeSize() < static_cast<uint64_t>(size)) {
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
          return (_status == StatusType::SEALED ||
                  _status == StatusType::COLLECTION_REQUESTED);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the logfile can be removed
////////////////////////////////////////////////////////////////////////////////

        inline bool canBeRemoved () const {
          return (_status == StatusType::COLLECTED && 
                  _collectQueueSize == 0 && 
                  _users == 0);
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

        std::string statusText () const {
          return statusText(status());
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
/// @brief change the logfile status, without assertions
////////////////////////////////////////////////////////////////////////////////

        void forceStatus (StatusType status) {
          _status = status;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief change the logfile status, with assertions
////////////////////////////////////////////////////////////////////////////////

        void setStatus (StatusType status) {
          switch (status) {
            case StatusType::UNKNOWN:
            case StatusType::EMPTY:
              TRI_ASSERT(false);
              break;

            case StatusType::OPEN:
              TRI_ASSERT(_status == StatusType::EMPTY);
              break;

            case StatusType::SEAL_REQUESTED:
              TRI_ASSERT(_status == StatusType::OPEN);
              break;

            case StatusType::SEALED:
              TRI_ASSERT(_status == StatusType::SEAL_REQUESTED);
              break;

            case StatusType::COLLECTION_REQUESTED:
              TRI_ASSERT(_status == StatusType::SEALED);
              break;

            case StatusType::COLLECTED:
              TRI_ASSERT(_status == StatusType::COLLECTION_REQUESTED);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the number of collect operations waiting
////////////////////////////////////////////////////////////////////////////////

        inline void increaseCollectQueueSize () {
          ++_collectQueueSize;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the number of collect operations waiting
////////////////////////////////////////////////////////////////////////////////

        inline void decreaseCollectQueueSize () {
          --_collectQueueSize;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief use a logfile - while there are users, the logfile cannot be 
/// deleted
////////////////////////////////////////////////////////////////////////////////

        inline void use () {
          ++_users;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief release a logfile - while there are users, the logfile cannot be 
/// deleted
////////////////////////////////////////////////////////////////////////////////

        inline void release () {
          TRI_ASSERT_EXPENSIVE(_users > 0);
          --_users;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a legend in the cache
////////////////////////////////////////////////////////////////////////////////

        void* lookupLegend (TRI_voc_cid_t cid, TRI_shape_sid_t sid) {
          CidSid cs(cid,sid);
          READ_LOCKER(_legendCacheLock);
          auto it = _legendCache.find(cs);
          if (it != _legendCache.end()) {
            return it->second;
          }
          else {
            return nullptr;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief cache a legend
////////////////////////////////////////////////////////////////////////////////

        void cacheLegend (TRI_voc_cid_t cid, TRI_shape_sid_t sid, void* l) {
          CidSid cs(cid,sid);
          WRITE_LOCKER(_legendCacheLock);
          auto it = _legendCache.find(cs);
          if (it == _legendCache.end()) {
            _legendCache.insert(make_pair(cs,l));
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the logfile id
////////////////////////////////////////////////////////////////////////////////

        Logfile::IdType const _id;

////////////////////////////////////////////////////////////////////////////////
/// @brief the number of logfile users
////////////////////////////////////////////////////////////////////////////////

        std::atomic<int32_t> _users;

////////////////////////////////////////////////////////////////////////////////
/// @brief the datafile entry
////////////////////////////////////////////////////////////////////////////////

        TRI_datafile_t* _df;

////////////////////////////////////////////////////////////////////////////////
/// @brief logfile status
////////////////////////////////////////////////////////////////////////////////

        StatusType _status;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of collect operations waiting
////////////////////////////////////////////////////////////////////////////////

        std::atomic<int64_t> _collectQueueSize;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief legend cache, key type with hash function
////////////////////////////////////////////////////////////////////////////////

      private:

        struct CidSid {
          TRI_voc_cid_t cid;
          TRI_shape_sid_t sid;
          CidSid (TRI_voc_cid_t c, TRI_shape_sid_t s)
            : cid(c), sid(s) {}
          bool operator== (CidSid const& a) const {
            return this->cid == a.cid && this->sid == a.sid;
          }
        };

        struct CidSidHash {
          size_t operator() (CidSid const& cs) const {
            return   std::hash<TRI_voc_cid_t>()(cs.cid) 
                   ^ std::hash<TRI_shape_sid_t>()(cs.sid);
          }
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief legend cache
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<CidSid, void*, CidSidHash> _legendCache;

////////////////////////////////////////////////////////////////////////////////
/// @brief legend cache, lock
////////////////////////////////////////////////////////////////////////////////

        basics::ReadWriteLock _legendCacheLock;

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
