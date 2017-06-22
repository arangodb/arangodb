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

#include "MMFilesDatafile.h"
#include "ApplicationFeatures/PageSizeFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/encoding.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "VocBase/ticks.h"

#include <sstream>
#include <iomanip>

// #define DEBUG_DATAFILE 1

using namespace arangodb;
using namespace arangodb::basics;

namespace {

/// @brief create a hex string representation of the value
static std::string hexValue(uint64_t value) {
  static const uint64_t Bits[] = { 56, 48, 40, 32, 24, 16, 8, 0 };

  std::string line("0x");
  for (uint64_t i = 0; i < 8; ++i) {
    uint8_t c = static_cast<uint8_t>((static_cast<uint64_t>(value) >> Bits[i]) & 0xFFULL);
    uint8_t n1 = c >> 4;
    uint8_t n2 = c & 0x0F;

    line.push_back((n1 < 10) ? ('0' + n1) : 'A' + n1 - 10);
    line.push_back((n2 < 10) ? ('0' + n2) : 'A' + n2 - 10);
  }

  return line;
}

/// @brief calculate a CRC value the same way as ArangoDB 2.8 did
static TRI_voc_crc_t Crc28(TRI_voc_crc_t crc, void const* data, size_t length) {
  static TRI_voc_crc_t const CrcPolynomial = 0xEDB88320; 
  unsigned char* current = (unsigned char*) data;   
  while (length--) {
    crc ^= *current++;

    for (unsigned int i = 0; i < 8; ++i) {
      if (crc & 1) {
        crc = (crc >> 1) ^ CrcPolynomial;
      } else {
        crc = crc >> 1;         
      }
    }
  }   
  return crc;
}

/// @brief check if a marker appears to be created by ArangoDB 2.8
static bool IsMarker28(void const* marker) {
  struct Marker28 {
    TRI_voc_size_t       _size; 
    TRI_voc_crc_t        _crc;     
    uint32_t             _type;   
#ifdef TRI_PADDING_32
    char _padding_df_marker[4];
#endif
    TRI_voc_tick_t _tick;     
  };

  TRI_voc_size_t zero = 0;
  off_t o = offsetof(Marker28, _crc);
  size_t n = sizeof(TRI_voc_crc_t);

  char const* ptr = static_cast<char const*>(marker);
  Marker28 const* m = static_cast<Marker28 const*>(marker);

  TRI_voc_crc_t crc = TRI_InitialCrc32();

  crc = Crc28(crc, ptr, o);
  crc = Crc28(crc, (char*) &zero, n);
  crc = Crc28(crc, ptr + o + n, m->_size - o - n);

  crc = TRI_FinalCrc32(crc);

  return crc == m->_crc;
}

/// @brief calculates the actual CRC of a marker, without bounds checks
static TRI_voc_crc_t CalculateCrcValue(MMFilesMarker const* marker) {
  TRI_voc_size_t zero = 0;
  off_t o = marker->offsetOfCrc();
  size_t n = sizeof(TRI_voc_crc_t);

  char const* ptr = reinterpret_cast<char const*>(marker);

  TRI_voc_crc_t crc = TRI_InitialCrc32();

  crc = TRI_BlockCrc32(crc, ptr, o);
  crc = TRI_BlockCrc32(crc, (char*)&zero, n);
  crc = TRI_BlockCrc32(crc, ptr + o + n, marker->getSize() - o - n);

  crc = TRI_FinalCrc32(crc);

  return crc;
}

/// @brief checks a CRC of a marker, with bounds checks
static bool CheckCrcMarker(MMFilesMarker const* marker, char const* end) {
  TRI_voc_size_t const size = marker->getSize();

  if (size < sizeof(MMFilesMarker)) {
    return false;
  }

  if (reinterpret_cast<char const*>(marker) + size > end) {
    return false;
  }

  auto expected = CalculateCrcValue(marker);
  return marker->getCrc() == expected;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-abc\.ending$/, where abc is
/// a number, and type and ending are arbitrary letters
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetNumericFilenamePart(char const* filename) {
  char const* pos1 = strrchr(filename, '.');

  if (pos1 == nullptr) {
    return 0;
  }

  char const* pos2 = strrchr(filename, '-');

  if (pos2 == nullptr || pos2 > pos1) {
    return 0;
  }

  return StringUtils::uint64(pos2 + 1, pos1 - pos2 - 1);
}

}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new anonymous datafile
///
/// this is only supported on certain platforms (Linux, MacOS)
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_ANONYMOUS_MMAP

static MMFilesDatafile* CreateAnonymousDatafile(TRI_voc_fid_t fid,
                                               TRI_voc_size_t maximalSize) {
#ifdef TRI_MMAP_ANONYMOUS
  // fd -1 is required for "real" anonymous regions
  int fd = -1;
  int flags = TRI_MMAP_ANONYMOUS | MAP_SHARED;
#else
  // ugly workaround if MAP_ANONYMOUS is not available
  int fd = TRI_TRACKED_OPEN_FILE("/dev/zero", O_RDWR | TRI_O_CLOEXEC);

  if (fd == -1) {
    return nullptr;
  }

  int flags = MAP_PRIVATE;
#endif

  // memory map the data
  void* data;
  void* mmHandle;
  int res = TRI_MMFile(nullptr, maximalSize, PROT_WRITE | PROT_READ, flags,
                       fd, &mmHandle, 0, &data);

#ifdef MAP_ANONYMOUS
// nothing to do
#else
  // close auxilliary file
  TRI_TRACKED_CLOSE_FILE(fd);
  fd = -1;
#endif

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot memory map anonymous region: " << TRI_last_error();
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "The database directory might reside on a shared folder "
                "(VirtualBox, VMWare) or an NFS "
                "mounted volume which does not allow memory mapped files.";
    return nullptr;
  }

  return new MMFilesDatafile(StaticStrings::Empty, fd, mmHandle, maximalSize, 0, fid, static_cast<char*>(data));
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new physical datafile
////////////////////////////////////////////////////////////////////////////////

static MMFilesDatafile* CreatePhysicalDatafile(std::string const& filename,
                                              TRI_voc_fid_t fid,
                                              TRI_voc_size_t maximalSize) {
  TRI_ASSERT(!filename.empty());

  int fd = TRI_CreateDatafile(filename, maximalSize);

  if (fd < 0) {
    // an error occurred
    return nullptr;
  }

  // memory map the data
  void* data;
  void* mmHandle;
  int flags = MAP_SHARED;
#ifdef __linux__
  // try populating the mapping already
  flags |= MAP_POPULATE;
#endif
  int res = TRI_MMFile(0, maximalSize, PROT_WRITE | PROT_READ, flags, fd,
                       &mmHandle, 0, &data);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    TRI_TRACKED_CLOSE_FILE(fd);

    // remove empty file
    TRI_UnlinkFile(filename.c_str());

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot memory map file '" << filename << "': '" << TRI_errno_string(res) << "'";
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "The database directory might reside on a shared folder "
                "(VirtualBox, VMWare) or an NFS-mounted volume which does not allow memory mapped files.";
    return nullptr;
  }

  // create datafile structure
  try {
    return new MMFilesDatafile(filename, fd, mmHandle, maximalSize, 0, fid, static_cast<char*>(data));
  } catch (...) {
    TRI_TRACKED_CLOSE_FILE(fd);
    return nullptr;
  }
}

/// @brief whether or not a datafile is empty
int MMFilesDatafile::judge(std::string const& filename) {
  off_t filesize = basics::FileUtils::size(filename);

  if (filesize == 0) {
    // empty logfile
    return TRI_ERROR_ARANGO_DATAFILE_EMPTY;
  }

  if (filesize < static_cast<off_t>(256 * sizeof(uint64_t))) {
    // too small
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  int fd = TRI_TRACKED_OPEN_FILE(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);

  if (fd < 0) {
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  uint64_t buffer[256];

  if (!TRI_ReadPointer(fd, &buffer, 256 * sizeof(uint64_t))) {
    TRI_TRACKED_CLOSE_FILE(fd);
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  uint64_t* ptr = buffer;
  uint64_t* end = buffer + 256;

  while (ptr < end) {
    if (*ptr != 0) {
      TRI_TRACKED_CLOSE_FILE(fd);
      return TRI_ERROR_NO_ERROR;
    }
    ++ptr;
  }

  TRI_TRACKED_CLOSE_FILE(fd);
  return TRI_ERROR_ARANGO_DATAFILE_EMPTY;
}

/// @brief creates either an anonymous or a physical datafile
MMFilesDatafile* MMFilesDatafile::create(std::string const& filename, TRI_voc_fid_t fid,
                                   TRI_voc_size_t maximalSize,
                                   bool withInitialMarkers) {
  size_t pageSize = PageSizeFeature::getPageSize();

  TRI_ASSERT(pageSize >= 256);

  // use multiples of page-size
  maximalSize =
      (TRI_voc_size_t)(((maximalSize + pageSize - 1) / pageSize) * pageSize);

  // sanity check maximal size
  if (sizeof(MMFilesDatafileHeaderMarker) + sizeof(MMFilesDatafileFooterMarker) >
      maximalSize) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create datafile, maximal size '" << maximalSize << "' is too small";
    TRI_set_errno(TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL);

    return nullptr;
  }

  // create either an anonymous or a physical datafile
  std::unique_ptr<MMFilesDatafile> datafile;
  if (filename.empty()) {
#ifdef TRI_HAVE_ANONYMOUS_MMAP
    datafile.reset(CreateAnonymousDatafile(fid, maximalSize));
#endif
  } else {
    datafile.reset(CreatePhysicalDatafile(filename, fid, maximalSize));
  }

  if (datafile == nullptr) {
    // an error occurred during creation
    return nullptr;
  }

  datafile->setState(TRI_DF_STATE_WRITE);

  if (withInitialMarkers) {
    int res = datafile->writeInitialHeaderMarker(fid, maximalSize);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot write header to datafile '" << datafile->getName() << "'";
      TRI_UNMMFile(const_cast<char*>(datafile->data()), datafile->initSize(), datafile->fd(),
                   &datafile->_mmHandle);

      datafile->close();
      return nullptr;
    }
  }

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "created datafile '" << datafile->getName() << "' of size " << maximalSize << " and page-size " << pageSize;

  return datafile.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name for a marker
////////////////////////////////////////////////////////////////////////////////

char const* TRI_NameMarkerDatafile(MMFilesMarkerType type) {
  switch (type) {
    // general markers
    case TRI_DF_MARKER_HEADER:
      return "datafile header";
    case TRI_DF_MARKER_FOOTER:
      return "footer";
    case TRI_DF_MARKER_BLANK:
      return "blank marker (used when repairing datafiles)";
    case TRI_DF_MARKER_COL_HEADER:
      return "collection header";
    case TRI_DF_MARKER_PROLOGUE:
      return "prologue";
    case TRI_DF_MARKER_VPACK_DOCUMENT:
      return "document";
    case TRI_DF_MARKER_VPACK_REMOVE:
      return "remove document";
    case TRI_DF_MARKER_VPACK_CREATE_COLLECTION:
      return "create collection";
    case TRI_DF_MARKER_VPACK_DROP_COLLECTION:
      return "drop collection";
    case TRI_DF_MARKER_VPACK_RENAME_COLLECTION:
      return "rename collection";
    case TRI_DF_MARKER_VPACK_CHANGE_COLLECTION:
      return "change collection";
    case TRI_DF_MARKER_VPACK_CREATE_INDEX:
      return "create index";
    case TRI_DF_MARKER_VPACK_DROP_INDEX:
      return "drop index";
    case TRI_DF_MARKER_VPACK_CREATE_DATABASE:
      return "create database";
    case TRI_DF_MARKER_VPACK_DROP_DATABASE:
      return "drop database";
    case TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION:
      return "begin transaction";
    case TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION:
      return "commit transaction";
    case TRI_DF_MARKER_VPACK_ABORT_TRANSACTION:
      return "abort transaction";
    case TRI_DF_MARKER_VPACK_CREATE_VIEW:
      return "create view";
    case TRI_DF_MARKER_VPACK_DROP_VIEW:
      return "drop view";
    case TRI_DF_MARKER_VPACK_CHANGE_VIEW:
      return "change view";

    default:
      return "unknown";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a marker is valid
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsValidMarkerDatafile(MMFilesMarker const* marker) {
  if (marker == nullptr) {
    return false;
  }

  // check marker type
  MMFilesMarkerType const type = marker->getType();

  if (type <= TRI_DF_MARKER_MIN) {
    // marker type is less than minimum allowed type value
    return false;
  }

  if (type >= TRI_DF_MARKER_MAX) {
    // marker type is greater than maximum allowed type value
    return false;
  }

  if (marker->getSize() >= MMFilesDatafileHelper::MaximalMarkerSize()) {
    // a single marker bigger than this limit seems unreasonable
    // note: this is an arbitrary limit
    return false;
  }

  return true;
}

/// @brief reserves room for an element, advances the pointer
/// note: maximalJournalSize is the collection's maximalJournalSize property,
/// which may be different from the size of the current datafile
/// some callers do not set the value of maximalJournalSize
int MMFilesDatafile::reserveElement(TRI_voc_size_t size, MMFilesMarker** position,
                                   TRI_voc_size_t maximalJournalSize) {
  *position = nullptr;
  size = encoding::alignedSize<TRI_voc_size_t>(size);

  if (_state != TRI_DF_STATE_WRITE) {
    if (_state == TRI_DF_STATE_READ) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot reserve marker, datafile is read-only";

      return TRI_ERROR_ARANGO_READ_ONLY;
    }

    return TRI_ERROR_ARANGO_ILLEGAL_STATE;
  }

  // check the maximal size
  if (size + MMFilesDatafileHelper::JournalOverhead() > _maximalSize) {
    // marker is bigger than journal size.
    // adding the marker to this datafile will not work

    if (maximalJournalSize <= _maximalSize) {
      // the collection property 'maximalJournalSize' is equal to
      // or smaller than the size of this datafile
      // creating a new file and writing the marker into it will not work either
      return TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE;
    }

    // if we get here, the collection's 'maximalJournalSize' property is
    // higher than the size of this datafile.
    // maybe the marker will fit into a new datafile with the bigger size?
    if (size + MMFilesDatafileHelper::JournalOverhead() > maximalJournalSize) {
      // marker still won't fit
      return TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE;
    }

    // fall-through intentional
  }

  // add the marker, leave enough room for the footer
  if (_currentSize + size + _footerSize > _maximalSize) {
    _lastError = TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_FULL);
    _full = true;

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "cannot write marker, not enough space";

    return TRI_ERROR_ARANGO_DATAFILE_FULL;
  }

  *position = reinterpret_cast<MMFilesMarker*>(_next);

  TRI_ASSERT(*position != nullptr);

  advanceWritePosition(size); 

  return TRI_ERROR_NO_ERROR;
}
  
void MMFilesDatafile::sequentialAccess() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_SEQUENTIAL);
}

void MMFilesDatafile::randomAccess() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_RANDOM);
}

void MMFilesDatafile::willNeed() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_WILLNEED);
}

void MMFilesDatafile::dontNeed() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_DONTNEED);
}

bool MMFilesDatafile::readOnly() {
  return (TRI_ProtectMMFile(_data, _initSize, PROT_READ, _fd) == TRI_ERROR_NO_ERROR);
}

bool MMFilesDatafile::readWrite() {
  return (TRI_ProtectMMFile(_data, _initSize, PROT_READ | PROT_WRITE, _fd) == TRI_ERROR_NO_ERROR);
}

void MMFilesDatafile::dontDump() {
  TRI_MMFileAdvise(_data, _initSize, TRI_MADVISE_DONTDUMP);
}

int MMFilesDatafile::lockInMemory() {
  TRI_ASSERT(!_lockedInMemory);
  int res = TRI_MMFileLock(_data, _initSize);

  if (res == TRI_ERROR_NO_ERROR) {
    _lockedInMemory = true;
  }
  return res;
}

int MMFilesDatafile::unlockFromMemory() {
  if (!_lockedInMemory) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_MMFileUnlock(_data, _initSize);

  if (res == TRI_ERROR_NO_ERROR) {
    _lockedInMemory = false;
  }
  return res;
}

/// @brief writes a marker to the datafile
/// this function will write the marker as-is, without any CRC or tick updates
int MMFilesDatafile::writeElement(void* position, MMFilesMarker const* marker, bool forceSync) {
  TRI_ASSERT(marker->getTick() > 0);
  TRI_ASSERT(marker->getSize() > 0);

  TRI_UpdateTicksDatafile(this, marker);

  if (_state != TRI_DF_STATE_WRITE) {
    if (_state == TRI_DF_STATE_READ) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot write marker, datafile is read-only";

      return TRI_ERROR_ARANGO_READ_ONLY;
    }

    return TRI_ERROR_ARANGO_ILLEGAL_STATE;
  }

  TRI_ASSERT(position != nullptr);

  // out of bounds check for writing into a datafile
  if (position == nullptr || position < (void*)_data ||
      position >= (void*)(_data + maximalSize())) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "logic error. writing out of bounds of datafile '" << getName() << "'";
    return TRI_ERROR_ARANGO_ILLEGAL_STATE;
  }

  memcpy(position, marker, static_cast<size_t>(marker->getSize()));

  if (forceSync) {
    bool ok = sync(static_cast<char const*>(position), reinterpret_cast<char const*>(position) + marker->getSize());

    if (!ok) {
      setState(TRI_DF_STATE_WRITE_ERROR);

      if (errno == ENOSPC) {
        _lastError = TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
      } else {
        _lastError = TRI_set_errno(TRI_ERROR_SYS_ERROR);
      }

      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "msync failed with: " << TRI_last_error();

      return _lastError;
    } else {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "msync succeeded " << (void*) position << ", size " << marker->getSize();
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update tick values for a datafile
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTicksDatafile(MMFilesDatafile* datafile,
                             MMFilesMarker const* marker) {
  MMFilesMarkerType const type = marker->getType();

  if (type != TRI_DF_MARKER_HEADER && type != TRI_DF_MARKER_FOOTER &&
      type != TRI_DF_MARKER_COL_HEADER) {
    // every marker but headers / footers counts

    TRI_voc_tick_t tick = marker->getTick();

    if (datafile->_tickMin == 0) {
      datafile->_tickMin = tick;
    }

    if (datafile->_tickMax < tick) {
      datafile->_tickMax = tick;
    }

    if (datafile->_dataMin == 0) {
      datafile->_dataMin = tick;
    }

    if (datafile->_dataMax < tick) {
      datafile->_dataMax = tick;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checksums and writes a marker to the datafile
////////////////////////////////////////////////////////////////////////////////

int MMFilesDatafile::writeCrcElement(void* position, MMFilesMarker* marker, bool forceSync) {
  TRI_ASSERT(marker->getTick() != 0);

  if (isPhysical()) {
    TRI_voc_crc_t crc = TRI_InitialCrc32();

    crc = TRI_BlockCrc32(crc, (char const*)marker, marker->getSize());
    marker->setCrc(TRI_FinalCrc32(crc));
  }

  return writeElement(position, marker, forceSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a datafile
/// also may set datafile's min/max tick values
/// deprecated
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateDatafile(MMFilesDatafile* datafile,
                         bool (*iterator)(MMFilesMarker const*, void*,
                                          MMFilesDatafile*),
                         void* data) {
  TRI_ASSERT(iterator != nullptr);

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "iterating over datafile '" << datafile->getName() << "', fid: " << datafile->fid();

  char const* ptr = datafile->data();
  char const* end = ptr + datafile->currentSize();

  if (datafile->state() != TRI_DF_STATE_READ &&
      datafile->state() != TRI_DF_STATE_WRITE) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
    return false;
  }

  TRI_voc_tick_t maxTick = 0;
  TRI_DEFER(TRI_UpdateTickServer(maxTick));

  while (ptr < end) {
    auto const* marker = reinterpret_cast<MMFilesMarker const*>(ptr);

    if (marker->getSize() == 0) {
      return true;
    }

    TRI_voc_tick_t tick = marker->getTick();

    if (tick > maxTick) {
      maxTick = tick;
    }

    // update the tick statistics
    TRI_UpdateTicksDatafile(datafile, marker);

    if (!iterator(marker, data, datafile)) {
      return false;
    }

    ptr += MMFilesDatafileHelper::AlignedMarkerSize<size_t>(marker);
  }

  return true;
}

/// @brief iterates over a datafile
/// also may set datafile's min/max tick values
bool TRI_IterateDatafile(MMFilesDatafile* datafile,
                         std::function<bool(MMFilesMarker const*, MMFilesDatafile*)> const& cb) {
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "iterating over datafile '" << datafile->getName() << "', fid: " << datafile->fid();

  char const* ptr = datafile->data();
  char const* end = ptr + datafile->currentSize();

  if (datafile->state() != TRI_DF_STATE_READ &&
      datafile->state() != TRI_DF_STATE_WRITE) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
    return false;
  }

  TRI_voc_tick_t maxTick = 0;
  TRI_DEFER(TRI_UpdateTickServer(maxTick));

  while (ptr < end) {
    auto const* marker = reinterpret_cast<MMFilesMarker const*>(ptr);

    if (marker->getSize() == 0) {
      return true;
    }
    
    TRI_voc_tick_t tick = marker->getTick();
    if (tick > maxTick) {
      maxTick = tick;
    }

    // update the tick statistics
    TRI_UpdateTicksDatafile(datafile, marker);

    if (!cb(marker, datafile)) {
      return false;
    }

    ptr += MMFilesDatafileHelper::AlignedMarkerSize<size_t>(marker);
  }

  return true;
}

/// @brief renames a datafile
int MMFilesDatafile::rename(std::string const& filename) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(isPhysical());
  TRI_ASSERT(!filename.empty());

  if (TRI_ExistsFile(filename.c_str())) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot overwrite datafile '" << filename << "'";

    _lastError = TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS;
    return TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS;
  }

  int res = TRI_RenameFile(_filename.c_str(), filename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    _state = TRI_DF_STATE_RENAME_ERROR;
    _lastError = TRI_ERROR_SYS_ERROR;

    return res;
  }

  _filename = filename;

  return TRI_ERROR_NO_ERROR;
}

/// @brief seals a datafile, writes a footer, sets it to read-only
int MMFilesDatafile::seal() {
  if (_state == TRI_DF_STATE_READ) {
    return TRI_ERROR_ARANGO_READ_ONLY;
  }

  if (_state != TRI_DF_STATE_WRITE) {
    return TRI_ERROR_ARANGO_ILLEGAL_STATE;
  }

  if (_isSealed) {
    return TRI_ERROR_ARANGO_DATAFILE_SEALED;
  }

  // set a proper tick value
  if (_tickMax == 0) {
    _tickMax = TRI_NewTickServer();
  }

  // create the footer
  MMFilesDatafileFooterMarker footer = MMFilesDatafileHelper::CreateFooterMarker(_tickMax);

  // reserve space and write footer to file
  _footerSize = 0;

  MMFilesMarker* position;
  int res = reserveElement(footer.base.getSize(), &position, 0);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT(position != nullptr);
    res = writeCrcElement(position, &footer.base, false);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // sync file
  bool ok = sync(_synced, reinterpret_cast<char const*>(_data) + _currentSize);

  if (!ok) {
    _state = TRI_DF_STATE_WRITE_ERROR;

    if (errno == ENOSPC) {
      _lastError = TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
    } else {
      _lastError = TRI_errno();
    }

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "msync failed with: " << TRI_last_error();
  }

  // everything is now synced
  _synced = _written;

  // intentionally ignore return value of protection here because this call
  // would only restrict further file accesses (which is not required
  // for ArangoDB to work) 
  readOnly();

  // seal datafile
  if (ok) {
    _isSealed = true;
    _state = TRI_DF_STATE_READ;
    // note: _initSize must remain constant
    TRI_ASSERT(_initSize == _maximalSize);
    _maximalSize = _currentSize;
  }

  if (!ok) {
    return _lastError;
  }

  if (isPhysical()) {
    // From now on we predict random access (until collection or compaction):
    randomAccess();
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief truncates a datafile and seals it
/// this is called from the recovery procedure only
int MMFilesDatafile::truncate(std::string const& path, TRI_voc_size_t position) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(!path.empty());

  std::unique_ptr<MMFilesDatafile> datafile(MMFilesDatafile::openHelper(path, true));

  if (datafile == nullptr) {
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  return datafile->truncateAndSeal(position);
}

/// @brief try to repair a datafile
bool MMFilesDatafile::tryRepair(std::string const& path) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(!path.empty());

  std::unique_ptr<MMFilesDatafile> datafile(MMFilesDatafile::openHelper(path, true));

  if (datafile == nullptr) {
    return false;
  }

  // set to read/write access
  if (!datafile->readWrite()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to change file protection for datafile '" << datafile->getName() << "'";
    return false;
  }

  return datafile->tryRepair();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief diagnoses a marker
////////////////////////////////////////////////////////////////////////////////

static std::string DiagnoseMarker(MMFilesMarker const* marker,
                                  char const* end) {
  std::ostringstream result;

  if (marker == nullptr) {
    return "marker is undefined. should not happen";
  }

  // check marker type
  MMFilesMarkerType type = marker->getType();

  if (type <= TRI_DF_MARKER_MIN) {
    // marker type is less than minimum allowed type value
    result << "marker type value (" << static_cast<int>(type)
           << ") is wrong. expecting value higher than " << TRI_DF_MARKER_MIN;
    return result.str();
  }

  if (type >= TRI_DF_MARKER_MAX) {
    // marker type is greater than maximum allowed type value
    result << "marker type value (" << static_cast<int>(type)
           << ") is wrong. expecting value less than " << TRI_DF_MARKER_MAX;
    return result.str();
  }

  TRI_voc_size_t size = marker->getSize();

  if (size >= MMFilesDatafileHelper::MaximalMarkerSize()) {
    // a single marker bigger than this size seems unreasonable
    // note: this is an arbitrary limit
    result << "marker size value (" << size
           << ") is wrong. expecting value less than "
           << MMFilesDatafileHelper::MaximalMarkerSize();
    return result.str();
  }

  if (size < sizeof(MMFilesMarker)) {
    result << "marker size is too small (" << size
           << "). expecting at least " << sizeof(MMFilesMarker) << " bytes";
    return result.str();
  }

  if (reinterpret_cast<char const*>(marker) + size > end) {
    return "marker size is beyond end of datafile";
  }

  TRI_voc_crc_t crc = CalculateCrcValue(marker);

  if (marker->getCrc() == crc) {
    return "crc checksum is correct";
  }
   
  result << "crc checksum (hex " << std::hex << marker->getCrc()
         << ") is wrong. expecting (hex " << std::hex << crc << ")";

  return result.str();
}

MMFilesDatafile::MMFilesDatafile(std::string const& filename, int fd, void* mmHandle, TRI_voc_size_t maximalSize,
                               TRI_voc_size_t currentSize, TRI_voc_fid_t fid, char* data)
        : _filename(filename),
          _fid(fid),
          _state(TRI_DF_STATE_READ),
          _fd(fd),
          _mmHandle(mmHandle),
          _initSize(maximalSize), 
          _maximalSize(maximalSize), 
          _currentSize(currentSize),
          _footerSize(sizeof(MMFilesDatafileFooterMarker)),
          _full(false),
          _isSealed(false),
          _lockedInMemory(false),
          _data(data),
          _next(data + currentSize),
          _tickMin(0),
          _tickMax(0),
          _dataMin(0),
          _dataMax(0),
          _lastError(TRI_ERROR_NO_ERROR),
          _synced(data),
          _written(nullptr) {
  // filename is a string for physical datafiles, and NULL for anonymous regions
  // fd is a positive value for physical datafiles, and -1 for anonymous regions
  if (filename.empty()) {
    TRI_ASSERT(fd == -1);
  } else {
    TRI_ASSERT(fd >= 0);

    // Advise OS that sequential access is going to happen:
    sequentialAccess();
  }
  dontDump();
}
  
MMFilesDatafile::~MMFilesDatafile() {
  try {
    this->close();
  } catch (...) {
    // silently continue as this is the destructor
  }
}

/// @brief return the name of a datafile
std::string MMFilesDatafile::getName() const {
  if (_filename.empty()) {
    // anonymous regions do not have a filename
    return "anonymous region";
  }

  // return name of the physical file
  return _filename;
}

/// @brief close a datafile
int MMFilesDatafile::close() {
  if (_state == TRI_DF_STATE_READ ||
      _state == TRI_DF_STATE_WRITE ||
      _state == TRI_DF_STATE_OPEN_ERROR) {
    int res = TRI_UNMMFile(_data, _initSize, _fd, &_mmHandle);

    if (res != TRI_ERROR_NO_ERROR) {
      // leave file open here as it will still be memory-mapped
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "munmap failed with: " << res;
      _state = TRI_DF_STATE_WRITE_ERROR;
      _lastError = res;
      return res;
    }

    if (isPhysical()) {
      TRI_ASSERT(_fd >= 0);
      int res = TRI_TRACKED_CLOSE_FILE(_fd);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to close datafile '" << getName() << "': " << res;
      }
    }

    _state = TRI_DF_STATE_CLOSED;
    _data = nullptr;
    _next = nullptr;
    _fd = -1;

    return TRI_ERROR_NO_ERROR;
  } 
  
  if (_state == TRI_DF_STATE_CLOSED) {
    TRI_ASSERT(_fd == -1);
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "closing an already closed datafile '" << getName() << "'";
    return TRI_ERROR_NO_ERROR;
  } 
        
  LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "attempting to close datafile with an invalid state";
  
  return TRI_ERROR_ARANGO_ILLEGAL_STATE;
}

/// @brief sync the data of a datafile
bool MMFilesDatafile::sync(char const* begin, char const* end) {
  if (!isPhysical()) {
    // anonymous regions do not need to be synced
    return true;
  }

  TRI_ASSERT(_fd >= 0);

  if (begin == end) {
    // no need to sync
    return true;
  }

  return TRI_MSync(_fd, begin, end);
}
 
/// @brief truncates a datafile
/// Create a truncated datafile, seal it and rename the old.
int MMFilesDatafile::truncateAndSeal(TRI_voc_size_t position) {
  TRI_ERRORBUF;
  void* data;
  void* mmHandle;

  // this function must not be called for non-physical datafiles
  TRI_ASSERT(isPhysical());
  size_t pageSize = PageSizeFeature::getPageSize();

  // use multiples of page-size
  size_t maximalSize =
      ((position + sizeof(MMFilesDatafileFooterMarker) + pageSize - 1) / pageSize) *
      pageSize;

  // sanity check
  if (sizeof(MMFilesDatafileHeaderMarker) + sizeof(MMFilesDatafileFooterMarker) >
      maximalSize) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create datafile '" << getName() << "', maximal size " << maximalSize << " is too small";
    return TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL;
  }

  // open the file
  std::string filename = arangodb::basics::FileUtils::buildFilename(getName(), ".new");

  int fd =
      TRI_TRACKED_CREATE_FILE(filename.c_str(), O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                 S_IRUSR | S_IWUSR);

  if (fd < 0) {
    TRI_SYSTEM_ERROR();
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create new datafile '" << filename << "': " << TRI_GET_ERRORBUF;

    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  // go back to the beginning of the file
  TRI_lseek_t offset = TRI_LSEEK(fd, (TRI_lseek_t)(maximalSize - 1), SEEK_SET);

  if (offset == (TRI_lseek_t)-1) {
    TRI_SYSTEM_ERROR();
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_TRACKED_CLOSE_FILE(fd);

    // remove empty file
    TRI_UnlinkFile(filename.c_str());

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot seek in new datafile '" << filename << "': " << TRI_GET_ERRORBUF;

    return TRI_ERROR_SYS_ERROR;
  }

  char zero = 0;
  long written = TRI_WRITE(fd, &zero, 1);
  if (written < 0) {
    TRI_SYSTEM_ERROR();
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_TRACKED_CLOSE_FILE(fd);

    // remove empty file
    TRI_UnlinkFile(filename.c_str());

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create datafile '" << filename << "': " << TRI_GET_ERRORBUF;

    return TRI_ERROR_SYS_ERROR;
  }

  // memory map the data
  int res = TRI_MMFile(0, maximalSize, PROT_WRITE | PROT_READ, MAP_SHARED, fd,
                       &mmHandle, 0, &data);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_SYSTEM_ERROR();
    TRI_set_errno(res);
    TRI_TRACKED_CLOSE_FILE(fd);

    // remove empty file
    TRI_UnlinkFile(filename.c_str());

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot memory map file '" << filename << "': " << TRI_GET_ERRORBUF;
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "The database directory might reside on a shared folder "
                "(VirtualBox, VMWare) or an NFS "
                "mounted volume which does not allow memory mapped files.";

    return TRI_errno();
  }

  // copy the data
  memcpy(data, _data, position);

  // patch the datafile structure
  res = TRI_UNMMFile(_data, _initSize, _fd, &_mmHandle);

  if (res < 0) {
    TRI_TRACKED_CLOSE_FILE(_fd);
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "munmap failed with: " << res;
    return res;
  }

  // .............................................................................................
  // For windows: Mem mapped files use handles
  // the datafile->_mmHandle handle object has been closed in the underlying
  // TRI_UNMMFile(...) call above so we do not need to close it for the
  // associated file below
  // .............................................................................................

  TRI_TRACKED_CLOSE_FILE(_fd);

  _data = static_cast<char*>(data);
  _next = (char*)(data) + position;
  _currentSize = position;
  // do not change _initSize!
  TRI_ASSERT(_initSize == _maximalSize);
  TRI_ASSERT(maximalSize <= _initSize);
  _maximalSize = static_cast<TRI_voc_size_t>(maximalSize);
  _fd = fd;
  _mmHandle = mmHandle;
  _state = TRI_DF_STATE_CLOSED;
  _full = false;
  _isSealed = false;
  _synced = static_cast<char*>(data);
  _written = _next;

  // rename files
  std::string oldname = arangodb::basics::FileUtils::buildFilename(_filename, ".corrupted");

  res = TRI_RenameFile(_filename.c_str(), oldname.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = TRI_RenameFile(filename.c_str(), _filename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // need to reset the datafile state here to write, otherwise the following
  // call will return an error
  _state = TRI_DF_STATE_WRITE;

  return seal();
}

/// @brief checks a datafile
bool MMFilesDatafile::check(bool ignoreFailures) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(isPhysical());
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "checking markers in datafile '" << getName() << "'";

  char const* ptr = _data;
  char const* end = ptr + _currentSize;
  char const* lastGood = nullptr;
  TRI_voc_size_t currentSize = 0;

  if (_currentSize == 0) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "current size is 0 in read-only datafile '" << getName() << "', trying to fix";

    end = _data + _maximalSize;
  }

  TRI_voc_tick_t maxTick = 0;
  TRI_DEFER(TRI_UpdateTickServer(maxTick));

  while (ptr < end) {
    MMFilesMarker const* marker = reinterpret_cast<MMFilesMarker const*>(ptr);
    TRI_voc_size_t const size = marker->getSize();
    TRI_voc_tick_t const tick = marker->getTick();
    MMFilesMarkerType const type = marker->getType();

#ifdef DEBUG_DATAFILE
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "MARKER: size " << size << ", tick " << tick << ", crc " << marker->getCrc() << ", type " << type;
#endif

    if (size == 0) {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "reached end of datafile '" << getName() << "' data, current size " << currentSize;

      _currentSize = currentSize;
      _next = _data + _currentSize;

      return true;
    }

    if (size < sizeof(MMFilesMarker)) {
      if (ignoreFailures) {
        return fix(currentSize);
      }
       
      _lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
      _currentSize = currentSize;
      _next = _data + _currentSize;
      _state = TRI_DF_STATE_OPEN_ERROR;

      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "marker in datafile '" << getName() << "' too small, size " << size << ", should be at least " << sizeof(MMFilesMarker);

      return false;
    }

    // prevent reading over the end of the file
    if (ptr + size > end) {
      if (ignoreFailures) {
        return fix(currentSize);
      }
       
      _lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
      _currentSize = currentSize;
      _next = _data + _currentSize;
      _state = TRI_DF_STATE_OPEN_ERROR;

      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "marker in datafile '" << getName() << "' points with size " << size << " beyond end of file";
      if (lastGood != nullptr) {
        LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "last good marker found at: " << hexValue(static_cast<uint64_t>(static_cast<uintptr_t>(lastGood - _data)));
      }
      printMarker(marker, static_cast<TRI_voc_size_t>(end - ptr), _data, end);

      return false;
    }

    // the following sanity check offers some, but not 100% crash-protection
    // when reading totally corrupted datafiles
    if (!TRI_IsValidMarkerDatafile(marker)) {
      if (type == 0 && size < 128) {
        // ignore markers with type 0 and a small size
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "ignoring suspicious marker in datafile '" << getName() << "': type: " << type << ", size: " << size;
      } else {
        if (ignoreFailures) {
          return fix(currentSize);
        }
         
        _lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
        _currentSize = currentSize;
        _next = _data + _currentSize;
        _state = TRI_DF_STATE_OPEN_ERROR;

        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "marker in datafile '" << getName() << "' is corrupt: type: " << type << ", size: " << size;
        if (lastGood != nullptr) {
          LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "last good marker found at: " << hexValue(static_cast<uint64_t>(static_cast<uintptr_t>(lastGood - _data)));
        }
        printMarker(marker, size, _data, end); 

        return false;
      }
    }

    if (type != 0) {
      bool ok = CheckCrcMarker(marker, end);

      if (!ok) {
        // CRC mismatch!
        bool nextMarkerOk = false;

        if (size > 0) {
          auto next = reinterpret_cast<char const*>(marker) + encoding::alignedSize<size_t>(size);
          auto p = next;

          if (p < end) {
            // check if the rest of the datafile is only followed by NULL bytes
            bool isFollowedByNullBytes = true;
            while (p < end) {
              if (*p != '\0') {
                isFollowedByNullBytes = false;
                break;
              }
              ++p;
            }

            if (isFollowedByNullBytes) {
              // only last marker in datafile was corrupt. fix the datafile in
              // place
              LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "datafile '" << getName() << "' automatically truncated at last marker";
              ignoreFailures = true;
            } else {
              // there is some other stuff following. now inspect it...
              TRI_ASSERT(next <= end);

              if (next < end) {
                // there is a next marker
                auto nextMarker =
                    reinterpret_cast<MMFilesMarker const*>(next);
                
                if (nextMarker->getType() != 0 &&
                    nextMarker->getSize() >= sizeof(MMFilesMarker) &&
                    next + nextMarker->getSize() <= end &&
                    TRI_IsValidMarkerDatafile(nextMarker) &&
                    CheckCrcMarker(nextMarker, end)) {
                  // next marker looks good.
                  nextMarkerOk = true;
                }
              } else {
                // EOF
                nextMarkerOk = true;
              }
            }
          }
        }

        if (!ignoreFailures) {
          _lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
          _currentSize = currentSize;
          _next = _data + _currentSize;
          _state = TRI_DF_STATE_OPEN_ERROR;

          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "crc mismatch found in datafile '" << getName() << "' of size "
                    << _maximalSize << ", at position " << currentSize;

          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "crc mismatch found inside marker of type '" << TRI_NameMarkerDatafile(marker) 
                    << "' and size " << size
                    << ". expected crc: " << CalculateCrcValue(marker) << ", actual crc: " << marker->getCrc();

          if (lastGood != nullptr) {
            LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "last good marker found at: " << hexValue(static_cast<uint64_t>(static_cast<uintptr_t>(lastGood - _data)));
          }
          printMarker(marker, size, _data, end); 

          if (nextMarkerOk) {
            LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "data directly following this marker looks ok so repairing the marker manually may recover it...";
            LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "to truncate the file at this marker, please restart the server with the parameter '--wal.ignore-logfile-errors true' if the error happening during WAL recovery, or with parameter '--database.ignore-datafile-errors true' if it happened after WAL recovery";
          } else {
            LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "data directly following this marker cannot be analyzed";
          }

          return false;
        }
       
        // ignore failures...   
        // truncate
        return fix(currentSize);
      }
    }

    if (tick > maxTick) {
      maxTick = tick;
    }

    size_t alignedSize = MMFilesDatafileHelper::AlignedMarkerSize<size_t>(marker);
    currentSize += static_cast<TRI_voc_size_t>(alignedSize);

    if (marker->getType() == TRI_DF_MARKER_FOOTER) {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "found footer, reached end of datafile '" << getName() << "', current size " << currentSize;

      _isSealed = true;
      _currentSize = currentSize;
      _next = _data + _currentSize;

      return true;
    }

    lastGood = ptr;
    ptr += alignedSize;
  }

  return true;
}

void MMFilesDatafile::printMarker(MMFilesMarker const* marker, TRI_voc_size_t size, char const* begin, char const* end) const {
  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "raw marker data following:";
  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "type: " << TRI_NameMarkerDatafile(marker) << ", size: " << marker->getSize() << ", crc: " << marker->getCrc();
  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "(expected layout: size (4 bytes), crc (4 bytes), type and tick (8 bytes), payload following)";
  char const* p = reinterpret_cast<char const*>(marker);
  char const* e = reinterpret_cast<char const*>(marker) + encoding::alignedSize<size_t>(size);

  if (e + 16 < end) {
    // add some extra bytes for following data
    e += 16;
  }

  std::string line;
  std::string raw;
  size_t printed = 0;
  while (p < e) {
    // print offset
    line.append(hexValue(static_cast<uint64_t>(static_cast<uintptr_t>(p - begin))));

    // print data
    line.append(": ");
    for (size_t i = 0; i < 16; ++i) {
      if (i == 8) {
        // separate groups of 8 bytes
        line.push_back(' ');
        raw.push_back(' ');
      }

      if (p >= e) {
        line.append("   ");
      } else {
        uint8_t c = static_cast<uint8_t>(*p++);
        uint8_t n1 = c >> 4;
        uint8_t n2 = c & 0x0F;

        line.push_back((n1 < 10) ? ('0' + n1) : 'A' + n1 - 10);
        line.push_back((n2 < 10) ? ('0' + n2) : 'A' + n2 - 10);
        line.push_back(' ');

        raw.push_back((c < 32 || c >= 127) ? '.' : static_cast<unsigned char>(c));

        ++printed;
      }
    }

    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << line << "  " << raw;
    line.clear();
    raw.clear();

    if (printed >= 2048) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "(output truncated due to excessive length)";
      break;
    }
  }
}

/// @brief fixes a corrupted datafile
bool MMFilesDatafile::fix(TRI_voc_size_t currentSize) {
  LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "datafile '" << getName() << "' is corrupted at position " << currentSize;

  LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "setting datafile '" << getName() << "' to read-only and ignoring all data from this file beyond this position";

  _currentSize = currentSize;
  TRI_ASSERT(_initSize == _maximalSize);
  TRI_ASSERT(currentSize <= _initSize);
  _maximalSize = static_cast<TRI_voc_size_t>(currentSize);
  _next = _data + _currentSize;
  _full = true;
  _state = TRI_DF_STATE_READ;
  _isSealed = true;

  return true;
}

/// @brief scans a datafile
DatafileScan MMFilesDatafile::scanHelper() {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(isPhysical());

  char* ptr = _data;
  char* end = _data + _currentSize;
  TRI_voc_size_t currentSize = 0;

  DatafileScan scan;

  scan.currentSize = _currentSize;
  scan.maximalSize = _maximalSize;

  if (_currentSize == 0) {
    end = _data + _maximalSize;
  }

  while (ptr < end) {
    MMFilesMarker* marker = reinterpret_cast<MMFilesMarker*>(ptr);

    DatafileScanEntry entry;
    entry.position = static_cast<TRI_voc_size_t>(ptr - _data);
    entry.size = marker->getSize();
    entry.realSize = static_cast<TRI_voc_size_t>(MMFilesDatafileHelper::AlignedMarkerSize<size_t>(marker));
    entry.tick = marker->getTick();
    entry.type = marker->getType();
    entry.status = 1;
    entry.typeName = TRI_NameMarkerDatafile(marker);

    if (marker->getSize() == 0 && marker->getCrc() == 0 && marker->getType() == 0 &&
        marker->getTick() == 0) {
      entry.status = 2;

      scan.endPosition = currentSize;
      scan.entries.emplace_back(entry);
      return scan;
    }

    ++scan.numberMarkers;

    if (marker->getSize() == 0) {
      entry.status = 3;

      scan.status = 2;
      scan.endPosition = currentSize;
      scan.entries.emplace_back(entry);
      return scan;
    }

    if (marker->getSize() < sizeof(MMFilesMarker)) {
      entry.status = 4;

      entry.diagnosis = DiagnoseMarker(marker, end);
      scan.endPosition = currentSize;
      scan.status = 3;

      scan.entries.emplace_back(entry);
      return scan;
    }

    if (!TRI_IsValidMarkerDatafile(marker)) {
      entry.status = 4;

      entry.diagnosis = DiagnoseMarker(marker, end);
      scan.endPosition = currentSize;
      scan.status = 3;

      scan.entries.emplace_back(entry);
      return scan;
    }

    bool ok = CheckCrcMarker(marker, end);

    if (!ok) {
      entry.status = 5;

      entry.diagnosis = DiagnoseMarker(marker, end);
      scan.status = 4;
    }

    MMFilesMarkerType const type = marker->getType();

    if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
        type == TRI_DF_MARKER_VPACK_REMOVE) {
      VPackSlice const slice(reinterpret_cast<char const*>(marker) + MMFilesDatafileHelper::VPackOffset(type));
      TRI_ASSERT(slice.isObject());
      entry.key = slice.get(StaticStrings::KeyString).copyString();
    }

    scan.entries.emplace_back(entry);

    size_t size = MMFilesDatafileHelper::AlignedMarkerSize<size_t>(marker);
    currentSize += static_cast<TRI_voc_size_t>(size);

    if (marker->getType() == TRI_DF_MARKER_FOOTER) {
      scan.endPosition = currentSize;
      scan.isSealed = true;
      return scan;
    }

    ptr += size;
  }

  return scan;
}

/// @brief create the initial datafile header marker
int MMFilesDatafile::writeInitialHeaderMarker(TRI_voc_fid_t fid, TRI_voc_size_t maximalSize) {
  // create the header
  MMFilesDatafileHeaderMarker header = MMFilesDatafileHelper::CreateHeaderMarker(
    maximalSize, static_cast<TRI_voc_tick_t>(fid));

  // reserve space and write header to file
  MMFilesMarker* position;
  int res = reserveElement(header.base.getSize(), &position, 0);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT(position != nullptr);
    res = writeCrcElement(position, &header.base, false);
  }

  return res;
}

/// @brief tries to repair a datafile
bool MMFilesDatafile::tryRepair() {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(isPhysical());

  char* ptr = _data;
  char* end = _data + _currentSize;

  if (_currentSize == 0) {
    end = _data + _maximalSize;
  }

  TRI_voc_size_t currentSize = 0;

  while (ptr < end) {
    MMFilesMarker* marker = reinterpret_cast<MMFilesMarker*>(ptr);
    TRI_voc_size_t const size = marker->getSize();

    if (size == 0) {
      // reached end
      return true;
    }

    if (size < sizeof(MMFilesMarker) || ptr + size > end) {
      // marker too small or too big
      return false;
    }

    if (!TRI_IsValidMarkerDatafile(marker)) {
      // unknown marker type
      return false;
    }

    if (marker->getType() != 0) {
      if (!CheckCrcMarker(marker, end)) {
        // CRC mismatch!
        auto next = reinterpret_cast<char const*>(marker) + size;
        auto p = next;

        if (p < end) {
          // check if the rest of the datafile is only followed by NULL bytes
          bool isFollowedByNullBytes = true;
          while (p < end) {
            if (*p != '\0') {
              isFollowedByNullBytes = false;
              break;
            }
            ++p;
          }

          if (isFollowedByNullBytes) {
            // only last marker in datafile was corrupt. fix the datafile in
            // place
            LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "truncating datafile '" << getName() << "' at position " << currentSize;
            int res = truncateAndSeal(currentSize);
            return (res == TRI_ERROR_NO_ERROR);
          }

          // there is some other stuff following. now inspect it...
          TRI_ASSERT(next <= end);

          if (next < end) {
            // there is a next marker
            auto nextMarker = reinterpret_cast<MMFilesMarker const*>(next);

            if (nextMarker->getType() != 0 &&
                nextMarker->getSize() >= sizeof(MMFilesMarker) &&
                next + nextMarker->getSize() <= end &&
                TRI_IsValidMarkerDatafile(nextMarker) &&
                CheckCrcMarker(nextMarker, end)) {
              // next marker looks good.

              // create a temporary buffer
              auto buffer = std::unique_ptr<char[]>(new char[size]);

              // create a new marker in the temporary buffer
              auto temp = reinterpret_cast<MMFilesMarker*>(buffer.get());
              MMFilesDatafileHelper::InitMarker(
                  reinterpret_cast<MMFilesMarker*>(buffer.get()), TRI_DF_MARKER_BLANK,
                  static_cast<uint32_t>(size));
              temp->setCrc(CalculateCrcValue(temp));

              // all done. now copy back the marker into the file
              memcpy(static_cast<void*>(ptr), buffer.get(),
                     static_cast<size_t>(size));

              buffer.reset(); // don't need the buffer anymore
              bool ok = sync(ptr, (ptr + size));

              if (ok) {
                LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "zeroed single invalid marker in datafile '" << getName() << "' at position " << currentSize;
              } else {
                LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not zero single invalid marker in datafile '" << getName() << "' at position " << currentSize;
                return false;
              }
            } else {
              // next marker looks broken, too.
              int res = truncateAndSeal(currentSize);
              return (res == TRI_ERROR_NO_ERROR);
            }
          }
        }
      }
    }

    size_t alignedSize = MMFilesDatafileHelper::AlignedMarkerSize<TRI_voc_size_t>(marker);
    currentSize += static_cast<TRI_voc_size_t>(alignedSize);

    if (marker->getType() == TRI_DF_MARKER_FOOTER) {
      return true;
    }

    ptr += alignedSize;
  }

  return true;
}

/// @brief opens an existing datafile
/// The datafile will be opened read-only if a footer is found
MMFilesDatafile* MMFilesDatafile::open(std::string const& filename, bool ignoreFailures) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(!filename.empty());

  std::unique_ptr<MMFilesDatafile> datafile(MMFilesDatafile::openHelper(filename, false));

  if (datafile == nullptr) {
    return nullptr;
  }

  // check the datafile by scanning markers
  bool ok = datafile->check(ignoreFailures);

  if (!ok) {
    TRI_UNMMFile(const_cast<char*>(datafile->data()), datafile->initSize(), datafile->fd(), &datafile->_mmHandle);
    TRI_TRACKED_CLOSE_FILE(datafile->fd());

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "datafile '" << datafile->getName() << "' is corrupt";
    // must free datafile here

    return nullptr;
  }

  // change to read-write if no footer has been found
  if (!datafile->_isSealed) {
    if (!datafile->readWrite()) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to change memory protection for memory backed by datafile '" << datafile->getName() << "'. please check file permissions and mount options.";
      return nullptr;
    }
    datafile->setState(TRI_DF_STATE_WRITE); 
  } 

  // Advise on sequential use:
  datafile->sequentialAccess();
  datafile->willNeed();

  return datafile.release();
}

/// @brief opens a datafile
MMFilesDatafile* MMFilesDatafile::openHelper(std::string const& filename, bool ignoreErrors) {
  TRI_ERRORBUF;

  // this function must not be called for non-physical datafiles
  TRI_ASSERT(!filename.empty());
  TRI_voc_fid_t fid = GetNumericFilenamePart(filename.c_str());

  // attempt to open a datafile file
  int fd = TRI_TRACKED_OPEN_FILE(filename.c_str(), O_RDWR | TRI_O_CLOEXEC);

  if (fd < 0) {
    TRI_SYSTEM_ERROR();
    TRI_set_errno(TRI_ERROR_SYS_ERROR);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot open datafile '" << filename << "': '" << TRI_GET_ERRORBUF << "'";

    return nullptr;
  }

  // compute the size of the file
  TRI_stat_t status;
  int res = TRI_FSTAT(fd, &status);

  if (res < 0) {
    TRI_SYSTEM_ERROR();
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_TRACKED_CLOSE_FILE(fd);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot get status of datafile '" << filename << "': " << TRI_GET_ERRORBUF;

    return nullptr;
  }

  // check that file is not too small
  TRI_voc_size_t size = static_cast<TRI_voc_size_t>(status.st_size);

  if (size < sizeof(MMFilesDatafileHeaderMarker) + sizeof(MMFilesDatafileFooterMarker)) {
    TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
    TRI_TRACKED_CLOSE_FILE(fd);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "datafile '" << filename << "' is corrupt, size is only " << size;

    return nullptr;
  }

  // read header from file
  char buffer[128];
  memset(&buffer[0], 0, sizeof(buffer)); 

  ssize_t len = sizeof(MMFilesDatafileHeaderMarker);

  ssize_t toRead = sizeof(buffer);
  if (toRead > static_cast<ssize_t>(status.st_size)) {
    toRead = static_cast<ssize_t>(status.st_size);
  }

  bool ok = TRI_ReadPointer(fd, &buffer[0], toRead);

  if (!ok) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot read datafile header from '" << filename << "': " << TRI_last_error();

    TRI_TRACKED_CLOSE_FILE(fd);
    return nullptr;
  }

  char const* ptr = reinterpret_cast<char*>(&buffer[0]);
  char const* end = static_cast<char const*>(ptr) + len;
  MMFilesDatafileHeaderMarker const* header = reinterpret_cast<MMFilesDatafileHeaderMarker const*>(&buffer[0]);

  // check CRC
  ok = CheckCrcMarker(reinterpret_cast<MMFilesMarker const*>(ptr), end);

  if (!ok) {
    if (IsMarker28(ptr)) {
      TRI_TRACKED_CLOSE_FILE(fd);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "datafile found from older version of ArangoDB. "
               << "Please dump data from that version with arangodump "
               << "and reload it into this ArangoDB instance with arangorestore";
      FATAL_ERROR_EXIT();
    }

    TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "corrupted datafile header read from '" << filename << "'";

    if (!ignoreErrors) {
      TRI_TRACKED_CLOSE_FILE(fd);
      return nullptr;
    }
  }

  // check the datafile version
  if (ok) {
    if (header->_version != TRI_DF_VERSION) {
      TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);

      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unknown datafile version '" << header->_version << "' in datafile '" << filename << "'";

      if (!ignoreErrors) {
        TRI_TRACKED_CLOSE_FILE(fd);
        return nullptr;
      }
    }
  }

  // check the maximal size
  if (size > header->_maximalSize) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "datafile '" << filename << "' has size " << size << ", but maximal size is " << header->_maximalSize;
  }

  // map datafile into memory
  void* data;
  void* mmHandle;
  res = TRI_MMFile(0, size, PROT_READ, MAP_SHARED, fd, &mmHandle, 0, &data);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    TRI_TRACKED_CLOSE_FILE(fd);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot memory map datafile '" << filename << "': " << TRI_errno_string(res);
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "The database directory might reside on a shared folder "
                "(VirtualBox, VMWare) or an NFS-mounted volume which does not allow memory mapped files.";
    return nullptr;
  }

  // create datafile structure
  try {
    return new MMFilesDatafile(filename, fd, mmHandle, size, size, fid, static_cast<char*>(data));
  } catch (...) {
    TRI_UNMMFile(data, size, fd, &mmHandle);
    TRI_TRACKED_CLOSE_FILE(fd);

    return nullptr;
  }
}

/// @brief returns information about the datafile
DatafileScan MMFilesDatafile::scan(std::string const& path) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(!path.empty());

  std::unique_ptr<MMFilesDatafile> datafile(MMFilesDatafile::openHelper(path, true));

  if (datafile != nullptr) {
    return datafile->scanHelper();
  } 
    
  DatafileScan scan;
  scan.currentSize = 0;
  scan.maximalSize = 0;
  scan.endPosition = 0;
  scan.numberMarkers = 0;

  scan.status = 5;
  scan.isSealed = false;

  return scan;
}
