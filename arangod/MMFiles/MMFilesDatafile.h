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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_MMFILES_DATAFILE_H
#define ARANGOD_MMFILES_MMFILES_DATAFILE_H 1

#include "Basics/Common.h"
#include "VocBase/vocbase.h"

struct MMFilesMarker;

/// @brief state of the datafile
enum TRI_df_state_e {
  TRI_DF_STATE_CLOSED = 1,       // datafile is closed
  TRI_DF_STATE_READ = 2,         // datafile is opened read only
  TRI_DF_STATE_WRITE = 3,        // datafile is opened read/append
  TRI_DF_STATE_OPEN_ERROR = 4,   // an error has occurred while opening
  TRI_DF_STATE_WRITE_ERROR = 5,  // an error has occurred while writing
  TRI_DF_STATE_RENAME_ERROR = 6  // an error has occurred while renaming
};

/// @brief type of the marker
enum MMFilesMarkerType : uint8_t {
  TRI_DF_MARKER_MIN = 9,  // not a real marker type,
                          // but used for bounds checking

  TRI_DF_MARKER_HEADER = 10,
  TRI_DF_MARKER_FOOTER = 11,
  TRI_DF_MARKER_BLANK = 12,
  
  TRI_DF_MARKER_COL_HEADER = 20,
  TRI_DF_MARKER_PROLOGUE = 25,

  TRI_DF_MARKER_VPACK_DOCUMENT = 30,
  TRI_DF_MARKER_VPACK_REMOVE = 31,

  TRI_DF_MARKER_VPACK_CREATE_COLLECTION = 40,
  TRI_DF_MARKER_VPACK_DROP_COLLECTION = 41,
  TRI_DF_MARKER_VPACK_RENAME_COLLECTION = 42,
  TRI_DF_MARKER_VPACK_CHANGE_COLLECTION = 43,
  TRI_DF_MARKER_VPACK_CREATE_INDEX = 50,
  TRI_DF_MARKER_VPACK_DROP_INDEX = 51,
  TRI_DF_MARKER_VPACK_CREATE_DATABASE = 60,
  TRI_DF_MARKER_VPACK_DROP_DATABASE = 61,
  TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION = 70,
  TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION = 71,
  TRI_DF_MARKER_VPACK_ABORT_TRANSACTION = 72,
  TRI_DF_MARKER_VPACK_CREATE_VIEW = 80,
  TRI_DF_MARKER_VPACK_DROP_VIEW = 81,
  TRI_DF_MARKER_VPACK_CHANGE_VIEW = 82,

  TRI_DF_MARKER_MAX  // again, this is not a real
                     // marker, but we use it for
                     // bounds checking
};

/// @brief scan result entry
/// status:
///   1 - entry ok
///   2 - empty entry
///   3 - empty size
///   4 - size too small
///   5 - CRC failed
struct DatafileScanEntry {
  DatafileScanEntry() 
      : position(0), size(0), realSize(0), tick(0), type(TRI_DF_MARKER_MIN), 
        status(0), typeName(nullptr) {}
  
  ~DatafileScanEntry() = default;

  TRI_voc_size_t position;
  TRI_voc_size_t size;
  TRI_voc_size_t realSize;
  TRI_voc_tick_t tick;

  MMFilesMarkerType type;
  uint32_t status;

  char const* typeName;
  std::string key;
  std::string diagnosis;
};

/// @brief scan result
struct DatafileScan {
  DatafileScan() 
      : currentSize(0), maximalSize(0), endPosition(0), numberMarkers(0),
        status(1), isSealed(false) {
    entries.reserve(2048);
  }

  TRI_voc_size_t currentSize;
  TRI_voc_size_t maximalSize;
  TRI_voc_size_t endPosition;
  TRI_voc_size_t numberMarkers;

  std::vector<DatafileScanEntry> entries;

  uint32_t status;
  bool isSealed;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Datafiles
///
/// All data is stored in datafiles. A set of datafiles forms a collection.
/// In the following sections the internal structure of a datafile is
/// described.
///
/// A datafile itself is a collection of blobs. These blobs can be shaped
/// JSON documents or any other information. All blobs have a header field,
/// call marker followed by the data of the blob itself.
///
/// @section DatafileMarker Datafile Marker
///
/// @copydetails MMFilesMarker
///
/// @copydetails MMFilesDatafileHeaderMarker
///
/// @copydetails MMFilesDatafileFooterMarker
///
/// A datafile is therefore structured as follows:
///
/// <table border>
///   <tr>
///     <td>MMFilesDatafileHeaderMarker</td>
///     <td>header entry</td>
///   </tr>
///   <tr>
///     <td>...</td>
///     <td>data entry</td>
///   </tr>
///   <tr>
///     <td>...</td>
///     <td>data entry</td>
///   </tr>
///   <tr>
///     <td>...</td>
///     <td>data entry</td>
///   </tr>
///   <tr>
///     <td>...</td>
///     <td>data entry</td>
///   </tr>
///   <tr>
///     <td>MMFilesDatafileFooterMarker</td>
///     <td>footer entry</td>
///   </tr>
/// </table>
///
/// @section WorkingWithDatafile Working With Datafiles
///
/// A datafile is created using the function @ref TRI_CreateDatafile.
///
/// @copydetails TRI_CreateDatafile
////////////////////////////////////////////////////////////////////////////////

/// @brief datafile version
#define TRI_DF_VERSION (2)

/// @brief datafile version
typedef uint32_t MMFilesDatafileVersionType;

/// @brief datafile
struct MMFilesDatafile {
  MMFilesDatafile(std::string const& filename, int fd, void* mmHandle, TRI_voc_size_t maximalSize,
                 TRI_voc_size_t currentsize, TRI_voc_fid_t fid, char* data);
  ~MMFilesDatafile();

  /// @brief return whether the datafile is a physical file (true) or an
  /// anonymous mapped region (false)
  inline bool isPhysical() const { return !_filename.empty(); }

  /// @brief return the name of a datafile
  std::string getName() const;

  /// @brief rename a datafile
  int rename(std::string const& filename);

  /// @brief truncates a datafile and seals it, only called by arango-dfdd
  static int truncate(std::string const& path, TRI_voc_size_t position);

  /// @brief whether or not a datafile is empty
  static int judge(std::string const& filename);

  /// @brief creates either an anonymous or a physical datafile
  static MMFilesDatafile* create(std::string const& filename, TRI_voc_fid_t fid,
                                TRI_voc_size_t maximalSize,
                                bool withInitialMarkers);

  /// @brief close a datafile
  int close();

  /// @brief sync the data of a datafile
  bool sync(char const* begin, char const* end); 

  /// @brief seals a datafile, writes a footer, sets it to read-only
  int seal();
  
  /// @brief scans a datafile
  static DatafileScan scan(std::string const& path);

  /// @brief try to repair a datafile
  static bool tryRepair(std::string const& path);
  
  /// @brief opens a datafile
  static MMFilesDatafile* open(std::string const& filename, bool ignoreErrors);

  /// @brief writes a marker to the datafile
  /// this function will write the marker as-is, without any CRC or tick updates
  int writeElement(void* position, MMFilesMarker const* marker, bool sync);

  /// @brief checksums and writes a marker to the datafile
  int writeCrcElement(void* position, MMFilesMarker* marker, bool sync);
  
  /// @brief reserves room for an element, advances the pointer
  int reserveElement(TRI_voc_size_t size, MMFilesMarker** position,
                     TRI_voc_size_t maximalJournalSize);
  
  void sequentialAccess();
  void randomAccess();
  void willNeed();
  void dontNeed();
  void dontDump();
  bool readOnly();
  bool readWrite();
  
  int lockInMemory();
  int unlockFromMemory();

  TRI_voc_fid_t fid() const { return _fid; }
  TRI_df_state_e state() const { return _state; }
  int fd() const { return _fd; }
  char const* data() const { return _data; }
  void* mmHandle() const { return _mmHandle; }
  TRI_voc_size_t initSize() const { return _initSize; }
  TRI_voc_size_t maximalSize() const { return _maximalSize; }
  TRI_voc_size_t currentSize() const { return _currentSize; }
  TRI_voc_size_t footerSize() const { return _footerSize; }
  
  void setState(TRI_df_state_e state) { _state = state; }
  
  bool isSealed() const { return _isSealed; }
  
  char* advanceWritePosition(size_t size) {
    char* old = _next;

    _next += size;
    _currentSize += static_cast<TRI_voc_size_t>(size);

    return old;
  }
  
 private:
  /// @brief returns information about the datafile
  DatafileScan scanHelper();
  
  int truncateAndSeal(TRI_voc_size_t position);

  /// @brief checks a datafile 
  bool check(bool ignoreFailures);

  /// @brief fixes a corrupted datafile
  bool fix(TRI_voc_size_t currentSize);

  /// @brief opens a datafile
  static MMFilesDatafile* openHelper(std::string const& filename, bool ignoreErrors);

  /// @brief create the initial datafile header marker
  int writeInitialHeaderMarker(TRI_voc_fid_t fid, TRI_voc_size_t maximalSize);

  /// @brief tries to repair a datafile
  bool tryRepair();

  void printMarker(MMFilesMarker const* marker, TRI_voc_size_t size, char const* begin, char const* end) const;
  
 private:
  std::string _filename;  // underlying filename
  TRI_voc_fid_t const _fid;  // datafile identifier
  TRI_df_state_e _state;  // state of the datafile (READ or WRITE)
  int _fd;                // underlying file descriptor

  void* _mmHandle;  // underlying memory map object handle (windows only)

  TRI_voc_size_t const _initSize; // initial size of the datafile (constant)
  TRI_voc_size_t _maximalSize;    // maximal size of the datafile (may be adjusted/reduced at runtime)
  TRI_voc_size_t _currentSize;    // current size of the datafile
  TRI_voc_size_t _footerSize;     // size of the final footer
  
  bool _full;  // at least one request was rejected because there is not enough
               // room
  bool _isSealed;  // true, if footer has been written
  bool _lockedInMemory;  // whether or not the datafile is locked in memory (mlock) 

 public:
  char* _data;  // start of the data array
  char* _next;  // end of the current data

  TRI_voc_tick_t _tickMin;  // minimum tick value contained
  TRI_voc_tick_t _tickMax;  // maximum tick value contained
  TRI_voc_tick_t _dataMin;  // minimum tick value of document/edge marker
  TRI_voc_tick_t _dataMax;  // maximum tick value of document/edge marker

  int _lastError;  // last (critical) error
  // .............................................................................
  // access to the following attributes must be protected by a _lock
  // .............................................................................

  char* _synced;   // currently synced upto, not including
  char* _written;  // currently written upto, not including
};

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile marker
///
/// All blobs of a datafile start with a header. The base structure for all
/// such headers is as follows:
///
/// <table border>
///   <tr>
///     <td>TRI_voc_size_t</td>
///     <td>_size</td>
///     <td>The total size of the blob. This includes the size of the the
///         marker and the data. In order to iterate through the datafile
///         you can read the TRI_voc_size_t entry _size and skip the next
///         _size - sizeof(TRI_voc_size_t) bytes.</td>
///   </tr>
///   <tr>
///     <td>TRI_voc_crc_t</td>
///     <td>_crc</td>
///     <td>A crc of the marker and the data. The zero is computed as if
///         the field _crc is equal to 0.</td>
///   </tr>
///   <tr>
///     <td>MMFilesMarkerType</td>
///     <td>_type</td>
///     <td>see @ref MMFilesMarkerType</td>
///   </tr>
///   <tr>
///     <td>TRI_voc_tick_t</td>
///     <td>_tick</td>
///     <td>A unique identifier of the current blob. The identifier is
///         unique within all datafiles of all collections. See
///         @ref TRI_voc_tick_t for details.</td>
///   </tr>
/// </table>
///
/// Note that the order is important: _size must be the first entry
/// and _crc the second.
////////////////////////////////////////////////////////////////////////////////

struct MMFilesMarker {
 private:
  TRI_voc_size_t _size;  // 4 bytes
  TRI_voc_crc_t _crc;    // 4 bytes, generated
  uint64_t _typeAndTick; // 8 bytes, including 1 byte for type and 7 bytes for tick
 
 public:
  MMFilesMarker() : _size(0), _crc(0), _typeAndTick(0) {}
  ~MMFilesMarker() {}

  inline off_t offsetOfSize() const noexcept {
    return offsetof(MMFilesMarker, _size);
  }
  inline off_t offsetOfCrc() const noexcept {
    return offsetof(MMFilesMarker, _crc);
  }
  inline off_t offsetOfTypeAndTick() const noexcept {
    return offsetof(MMFilesMarker, _typeAndTick);
  }
  inline TRI_voc_size_t getSize() const noexcept { return _size; }
  inline void setSize(TRI_voc_size_t size) noexcept { _size = size; }
  
  inline TRI_voc_crc_t getCrc() const noexcept { return _crc; }
  inline void setCrc(TRI_voc_crc_t crc) noexcept { _crc = crc; }
    
  static inline TRI_voc_tick_t makeTick(TRI_voc_tick_t tick) noexcept {
    return tick & 0x00ffffffffffffffULL;
  }

  inline TRI_voc_tick_t getTick() const noexcept { 
    return makeTick(static_cast<TRI_voc_tick_t>(_typeAndTick));
  }

  inline void setTick(TRI_voc_tick_t tick) noexcept { 
    _typeAndTick &= 0xff00000000000000ULL; 
    _typeAndTick |= makeTick(tick);
  }

  inline MMFilesMarkerType getType() const noexcept { 
    return static_cast<MMFilesMarkerType>((_typeAndTick & 0xff00000000000000ULL) >> 56); 
  }

  inline void setType(MMFilesMarkerType type) noexcept { 
    uint64_t t = static_cast<uint64_t>(type) << 56;
    _typeAndTick = makeTick(_typeAndTick);
    _typeAndTick |= t;
  }
   
  inline void setTypeAndTick(MMFilesMarkerType type, TRI_voc_tick_t tick) noexcept {
    uint64_t t = static_cast<uint64_t>(type) << 56;
    t |= makeTick(tick);
    _typeAndTick = t;
  }

};

static_assert(sizeof(MMFilesMarker) == 16, "invalid size for MMFilesMarker");

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile header marker
///
/// The first blob entry in a datafile is always a MMFilesDatafileHeaderMarker.
/// The header marker contains the version number of the datafile, its
/// maximal size and the creation time. There is no data payload.
///
/// <table border>
///   <tr>
///     <td>MMFilesDatafileVersionType</td>
///     <td>_version</td>
///     <td>The version of a datafile, see @ref MMFilesDatafileVersionType.</td>
///   </tr>
///   <tr>
///     <td>TRI_voc_size_t</td>
///     <td>_maximalSize</td>
///     <td>The maximal size to which a datafile can grow. If you
///         attempt to add more datafile to a datafile, then an
///         error TRI_ERROR_ARANGO_DATAFILE_FULL is returned.</td>
///   </tr>
///   <tr>
///     <td>TRI_voc_tick_t</td>
///     <td>_fid</td>
///     <td>The creation time of the datafile. This time is different
///         from the creation time of the blob entry stored in
///         base._tick.</td>
///   </tr>
/// </table>
////////////////////////////////////////////////////////////////////////////////

struct MMFilesDatafileHeaderMarker {
  MMFilesMarker base;  // 16 bytes

  MMFilesDatafileVersionType _version;    //  4 bytes
  TRI_voc_size_t _maximalSize;  //  4 bytes
  TRI_voc_tick_t _fid;          //  8 bytes
};

/// @brief datafile prologue marker
struct MMFilesPrologueMarker {
  MMFilesMarker base;  // 16 bytes

  TRI_voc_tick_t _databaseId; // 8 bytes
  TRI_voc_cid_t _collectionId; // 8 bytes
};

/// @brief datafile footer marker
struct MMFilesDatafileFooterMarker {
  MMFilesMarker base;  // 16 bytes
};

/// @brief document datafile header marker
struct MMFilesCollectionHeaderMarker {
  MMFilesMarker base;  // 16 bytes

  TRI_voc_cid_t _cid;  //  8 bytes
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name for a marker
////////////////////////////////////////////////////////////////////////////////

char const* TRI_NameMarkerDatafile(MMFilesMarkerType);

static inline char const* TRI_NameMarkerDatafile(MMFilesMarker const* marker) {
  return TRI_NameMarkerDatafile(marker->getType());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a marker is valid
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsValidMarkerDatafile(MMFilesMarker const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief update tick values for a datafile
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTicksDatafile(MMFilesDatafile*, MMFilesMarker const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a datafile
/// also may set datafile's min/max tick values
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateDatafile(MMFilesDatafile*,
                         bool (*iterator)(MMFilesMarker const*, void*,
                                          MMFilesDatafile*),
                         void* data);
                             
bool TRI_IterateDatafile(MMFilesDatafile*,
                         std::function<bool(MMFilesMarker const*, MMFilesDatafile*)> const& cb);

#endif
