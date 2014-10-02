////////////////////////////////////////////////////////////////////////////////
/// @brief datafiles
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "datafile.h"

#include "Basics/conversions.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Basics/files.h"
#include "VocBase/server.h"


// #define DEBUG_DATAFILE 1

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the datafile is a physical file (true) or an
/// anonymous mapped region (false)
////////////////////////////////////////////////////////////////////////////////

static bool IsPhysicalDatafile (const TRI_datafile_t* const datafile) {
  return datafile->_filename != nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of a datafile
////////////////////////////////////////////////////////////////////////////////

static const char* GetNameDatafile (const TRI_datafile_t* const datafile) {
  if (datafile->_filename == nullptr) {
    // anonymous regions do not have a filename
    return "anonymous region";
  }

  // return name of the physical file
  return datafile->_filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close a datafile
////////////////////////////////////////////////////////////////////////////////

static void CloseDatafile (TRI_datafile_t* const datafile) {
  TRI_ASSERT(datafile->_state != TRI_DF_STATE_CLOSED);

  if (datafile->isPhysical(datafile)) {
    TRI_CLOSE(datafile->_fd);
  }

  datafile->_state = TRI_DF_STATE_CLOSED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a datafile
////////////////////////////////////////////////////////////////////////////////

static void DestroyDatafile (TRI_datafile_t* const datafile) {
  if (datafile->_filename != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, datafile->_filename);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sync the data of a datafile
////////////////////////////////////////////////////////////////////////////////

static bool SyncDatafile (const TRI_datafile_t* const datafile,
                          char const* begin,
                          char const* end) {
  // TODO: remove
  void** mmHandle = nullptr;

  if (datafile->_filename == nullptr) {
    // anonymous regions do not need to be synced
    return true;
  }

  TRI_ASSERT(datafile->_fd >= 0);

  if (begin == end) {
    // no need to sync
    return true;
  }

  return TRI_MSync(datafile->_fd, mmHandle, begin, end);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate the datafile to a specific length
////////////////////////////////////////////////////////////////////////////////

static int TruncateDatafile (TRI_datafile_t* const datafile, const off_t length) {
  if (datafile->isPhysical(datafile)) {
    // only physical files can be truncated
    return ftruncate(datafile->_fd, length);
  }

  // for anonymous regions, this is a non-op
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a CRC of a marker
////////////////////////////////////////////////////////////////////////////////

static bool CheckCrcMarker (TRI_df_marker_t const* marker) {
  TRI_voc_size_t zero = 0;
  off_t o = offsetof(TRI_df_marker_t, _crc);
  size_t n = sizeof(TRI_voc_crc_t);

  char const* ptr = (char const*) marker;

  if (marker->_size < sizeof(TRI_df_marker_t)) {
    return false;
  }

  TRI_voc_crc_t crc = TRI_InitialCrc32();

  crc = TRI_BlockCrc32(crc, ptr, o);
  crc = TRI_BlockCrc32(crc, (char*) &zero, n);
  crc = TRI_BlockCrc32(crc, ptr + o + n, marker->_size - o - n);

  crc = TRI_FinalCrc32(crc);

  return marker->_crc == crc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new sparse datafile
///
/// returns the file descriptor or -1 if the file cannot be created
////////////////////////////////////////////////////////////////////////////////

static int CreateSparseFile (char const* filename,
                             const TRI_voc_size_t maximalSize) {
  TRI_lseek_t offset;
  char zero;
  ssize_t res;
  int fd;

  // open the file
  fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);

    LOG_ERROR("cannot create datafile '%s': %s", filename, TRI_last_error());
    return -1;
  }

  // create sparse file
  offset = TRI_LSEEK(fd, (TRI_lseek_t) (maximalSize - 1), SEEK_SET);

  if (offset == (TRI_lseek_t) -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_CLOSE(fd);

    // remove empty file
    TRI_UnlinkFile(filename);

    LOG_ERROR("cannot seek in datafile '%s': '%s'", filename, TRI_last_error());
    return -1;
  }

  zero = 0;
  res = TRI_WRITE(fd, &zero, 1);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_CLOSE(fd);

    // remove empty file
    TRI_UnlinkFile(filename);

    LOG_ERROR("cannot create sparse datafile '%s': '%s'", filename, TRI_last_error());
    return -1;
  }

  return fd;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a datafile
////////////////////////////////////////////////////////////////////////////////

static void InitDatafile (TRI_datafile_t* datafile,
                          char* filename,
                          int fd,
                          void* mmHandle,
                          TRI_voc_size_t maximalSize,
                          TRI_voc_size_t currentSize,
                          TRI_voc_fid_t fid,
                          char* data) {

  // filename is a string for physical datafiles, and NULL for anonymous regions
  // fd is a positive value for physical datafiles, and -1 for anonymous regions
  if (filename == nullptr) {
    TRI_ASSERT(fd == -1);
  }
  else {
    TRI_ASSERT(fd >= 0);
  }

  datafile->_state       = TRI_DF_STATE_READ;
  datafile->_fid         = fid;

  datafile->_filename    = filename;
  datafile->_fd          = fd;
  datafile->_mmHandle    = mmHandle;

  datafile->_maximalSize = maximalSize;
  datafile->_currentSize = currentSize;
  datafile->_footerSize  = sizeof(TRI_df_footer_marker_t);

  datafile->_isSealed    = false;
  datafile->_lastError   = TRI_ERROR_NO_ERROR;

  datafile->_full        = false;

  datafile->_data        = data;
  datafile->_next        = data + currentSize;

  datafile->_synced      = data;
  datafile->_written     = nullptr;

  // reset tick aggregates
  datafile->_tickMin     = 0;
  datafile->_tickMax     = 0;
  datafile->_dataMin     = 0;
  datafile->_dataMax     = 0;

  // initialise function pointers
  datafile->isPhysical   = &IsPhysicalDatafile;
  datafile->getName      = &GetNameDatafile;
  datafile->close        = &CloseDatafile;
  datafile->destroy      = &DestroyDatafile;
  datafile->sync         = &SyncDatafile;
  datafile->truncate     = &TruncateDatafile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a datafile
///
/// Create a truncated datafile, seal it and rename the old.
////////////////////////////////////////////////////////////////////////////////

static int TruncateAndSealDatafile (TRI_datafile_t* datafile,
                                    TRI_voc_size_t vocSize) {
  char* oldname;
  char zero;
  int res;
  void* data;
  void* mmHandle;

  // this function must not be called for non-physical datafiles
  TRI_ASSERT(datafile->isPhysical(datafile));

  // use multiples of page-size
  size_t maximalSize = ((vocSize + sizeof(TRI_df_footer_marker_t) + PageSize - 1) / PageSize) * PageSize;

  // sanity check
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) > maximalSize) {
    LOG_ERROR("cannot create datafile '%s', maximal size '%u' is too small", datafile->getName(datafile), (unsigned int) maximalSize);
    return TRI_set_errno(TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL);
  }

  // open the file
  char* filename = TRI_Concatenate2String(datafile->_filename, ".new");

  int fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    LOG_ERROR("cannot create new datafile '%s': '%s'", filename, TRI_last_error());
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  // create sparse file
  TRI_lseek_t offset = TRI_LSEEK(fd, (TRI_lseek_t) (maximalSize - 1), SEEK_SET);

  if (offset == (TRI_lseek_t) -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_CLOSE(fd);

    // remove empty file
    TRI_UnlinkFile(filename);

    LOG_ERROR("cannot seek in new datafile '%s': '%s'", filename, TRI_last_error());
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_ERROR_SYS_ERROR;
  }

  zero = 0;
  res = TRI_WRITE(fd, &zero, 1);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_CLOSE(fd);

    // remove empty file
    TRI_UnlinkFile(filename);

    LOG_ERROR("cannot create sparse datafile '%s': '%s'", filename, TRI_last_error());
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_ERROR_SYS_ERROR;
  }

  // memory map the data
  res = TRI_MMFile(0, maximalSize, PROT_WRITE | PROT_READ, MAP_SHARED, fd, &mmHandle, 0, &data);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    TRI_CLOSE(fd);

    // remove empty file
    TRI_UnlinkFile(filename);

    LOG_ERROR("cannot memory map file '%s': '%s'", filename, TRI_last_error());
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_errno();
  }

  // copy the data
  memcpy(data, datafile->_data, vocSize);

  // patch the datafile structure
  res = TRI_UNMMFile(datafile->_data, datafile->_maximalSize, datafile->_fd, &datafile->_mmHandle);

  if (res < 0) {
    TRI_CLOSE(datafile->_fd);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    LOG_ERROR("munmap failed with: %d", res);
    return res;
  }

  // .............................................................................................
  // For windows: Mem mapped files use handles
  // the datafile->_mmHandle handle object has been closed in the underlying
  // TRI_UNMMFile(...) call above so we do not need to close it for the associated file below
  // .............................................................................................

  TRI_CLOSE(datafile->_fd);

  datafile->_data = static_cast<char*>(data);
  datafile->_next = (char*)(data) + vocSize;
  datafile->_currentSize = vocSize;
  datafile->_maximalSize = static_cast<TRI_voc_size_t>(maximalSize);
  datafile->_fd = fd;
  datafile->_mmHandle = mmHandle;
  datafile->_state = TRI_DF_STATE_CLOSED;
  datafile->_full = false;
  datafile->_isSealed = false;
  datafile->_synced = static_cast<char*>(data);
  datafile->_written = datafile->_next;

  // rename files
  oldname = TRI_Concatenate2String(datafile->_filename, ".corrupted");

  res = TRI_RenameFile(datafile->_filename, oldname);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldname);

    return res;
  }

  res = TRI_RenameFile(filename, datafile->_filename);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldname);

    return res;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  TRI_FreeString(TRI_CORE_MEM_ZONE, oldname);

  // need to reset the datafile state here to write, otherwise the following call will return an error
  datafile->_state = TRI_DF_STATE_WRITE;

  return TRI_SealDatafile(datafile);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans a datafile
////////////////////////////////////////////////////////////////////////////////

static TRI_df_scan_t ScanDatafile (TRI_datafile_t const* datafile) {
  TRI_df_scan_t scan;
  TRI_df_scan_entry_t entry;
  TRI_voc_size_t currentSize;
  char* end;
  char* ptr;

  // this function must not be called for non-physical datafiles
  TRI_ASSERT(datafile->isPhysical(datafile));

  ptr = datafile->_data;
  end = datafile->_data + datafile->_currentSize;
  currentSize = 0;

  TRI_InitVector(&scan._entries, TRI_CORE_MEM_ZONE, sizeof(TRI_df_scan_entry_t));

  scan._currentSize = datafile->_currentSize;
  scan._maximalSize = datafile->_maximalSize;
  scan._numberMarkers = 0;
  scan._status = 1;
  scan._isSealed = false; // assume false

  if (datafile->_currentSize == 0) {
    end = datafile->_data + datafile->_maximalSize;
  }

  while (ptr < end) {
    TRI_df_marker_t* marker = (TRI_df_marker_t*) ptr;
    bool ok;
    size_t size;

    memset(&entry, 0, sizeof(entry));

    entry._position = (TRI_voc_size_t) (ptr - datafile->_data);
    entry._size = marker->_size;
    entry._realSize = TRI_DF_ALIGN_BLOCK(marker->_size);
    entry._tick = marker->_tick;
    entry._type = marker->_type;
    entry._status = 1;

    if (marker->_size == 0 && marker->_crc == 0 && marker->_type == 0 && marker->_tick == 0) {
      entry._status = 2;

      scan._endPosition = currentSize;

      TRI_PushBackVector(&scan._entries, &entry);
      return scan;
    }

    ++scan._numberMarkers;

    if (marker->_size == 0) {
      entry._status = 3;

      scan._status = 2;
      scan._endPosition = currentSize;

      TRI_PushBackVector(&scan._entries, &entry);
      return scan;
    }

    if (marker->_size < sizeof(TRI_df_marker_t)) {
      entry._status = 4;

      scan._endPosition = currentSize;
      scan._status = 3;

      TRI_PushBackVector(&scan._entries, &entry);
      return scan;
    }

    if (! TRI_IsValidMarkerDatafile(marker)) {
      entry._status = 4;

      scan._endPosition = currentSize;
      scan._status = 3;

      TRI_PushBackVector(&scan._entries, &entry);
      return scan;
    }

    ok = CheckCrcMarker(marker);

    if (! ok) {
      entry._status = 5;
      scan._status = 4;
    }

    TRI_PushBackVector(&scan._entries, &entry);

    size = TRI_DF_ALIGN_BLOCK(marker->_size);
    currentSize += (TRI_voc_size_t) size;

    if (marker->_type == TRI_DF_MARKER_FOOTER) {
      scan._endPosition = currentSize;
      scan._isSealed = true;
      return scan;
    }

    ptr += size;
  }

  return scan;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fixes a corrupted datafile
////////////////////////////////////////////////////////////////////////////////

static bool FixDatafile (TRI_datafile_t* datafile,
                         TRI_voc_size_t currentSize) {
  LOG_WARNING("datafile '%s' is corrupted at position %llu. setting it to read-only",
              datafile->getName(datafile),
              (unsigned long long) currentSize);

  datafile->_currentSize = currentSize;
  datafile->_maximalSize = static_cast<TRI_voc_size_t>(currentSize);
  datafile->_next = datafile->_data + datafile->_currentSize;
  datafile->_full = true;
  datafile->_state = TRI_DF_STATE_READ;
  datafile->_isSealed = true;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile
////////////////////////////////////////////////////////////////////////////////

static bool CheckDatafile (TRI_datafile_t* datafile,
                           bool ignoreFailures) {
  TRI_voc_size_t currentSize;
  char* end;
  char* ptr;

  // this function must not be called for non-physical datafiles
  TRI_ASSERT(datafile->isPhysical(datafile));

  ptr = datafile->_data;
  end = datafile->_data + datafile->_currentSize;
  currentSize = 0;

  if (datafile->_currentSize == 0) {
    LOG_WARNING("current size is 0 in read-only datafile '%s', trying to fix", datafile->getName(datafile));

    end = datafile->_data + datafile->_maximalSize;
  }

  while (ptr < end) {
    TRI_df_marker_t* marker = (TRI_df_marker_t*) ptr;
    size_t size;

#ifdef DEBUG_DATAFILE
    LOG_TRACE("MARKER: size %lu, tick %lx, crc %lx, type %u",
              (unsigned long) marker->_size,
              (unsigned long long) marker->_tick,
              (unsigned long) marker->_crc,
              (unsigned int) marker->_type);
#endif

    if (marker->_size == 0) {
      LOG_DEBUG("reached end of datafile '%s' data, current size %lu",
                datafile->getName(datafile),
                (unsigned long) currentSize);

      datafile->_currentSize = currentSize;
      datafile->_next = datafile->_data + datafile->_currentSize;

      return true;
    }

    if (marker->_size < sizeof(TRI_df_marker_t)) {
      datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
      datafile->_currentSize = currentSize;
      datafile->_next = datafile->_data + datafile->_currentSize;
      datafile->_state = TRI_DF_STATE_OPEN_ERROR;

      LOG_WARNING("marker in datafile '%s' too small, size %lu, should be at least %lu",
                  datafile->getName(datafile),
                  (unsigned long) marker->_size,
                  (unsigned long) sizeof(TRI_df_marker_t));

      return false;
    }

    // the following sanity check offers some, but not 100% crash-protection when reading
    // totally corrupted datafiles
    if (! TRI_IsValidMarkerDatafile(marker)) {
      if (marker->_type == 0 && marker->_size < 128) {
        // ignore markers with type 0 and a small size
        LOG_WARNING("ignoring suspicious marker in datafile '%s': type: %d, size: %lu",
                    datafile->getName(datafile),
                    (int) marker->_type,
                    (unsigned long) marker->_size);
      }
      else {
        if (ignoreFailures) {
          return FixDatafile(datafile, currentSize);
        }
        else {
          datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
          datafile->_currentSize = currentSize;
          datafile->_next = datafile->_data + datafile->_currentSize;
          datafile->_state = TRI_DF_STATE_OPEN_ERROR;

          LOG_WARNING("marker in datafile '%s' is corrupt: type: %d, size: %lu",
                      datafile->getName(datafile),
                      (int) marker->_type,
                      (unsigned long) marker->_size);
          return false;
        }
      }
    }

    if (marker->_type != 0) {
      bool ok = CheckCrcMarker(marker);

      if (! ok) {
        if (ignoreFailures) {
          return FixDatafile(datafile, currentSize);
        }
        else {
          datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
          datafile->_currentSize = currentSize;
          datafile->_next = datafile->_data + datafile->_currentSize;
          datafile->_state = TRI_DF_STATE_OPEN_ERROR;

          LOG_WARNING("crc mismatch found in datafile '%s'", datafile->getName(datafile));
          return false;
        }
      }
    }

    TRI_UpdateTickServer(marker->_tick);

    size = TRI_DF_ALIGN_BLOCK(marker->_size);
    currentSize += (TRI_voc_size_t) size;

    if (marker->_type == TRI_DF_MARKER_FOOTER) {
      LOG_DEBUG("found footer, reached end of datafile '%s', current size %lu",
                datafile->getName(datafile),
                (unsigned long) currentSize);

      datafile->_isSealed = true;
      datafile->_currentSize = currentSize;
      datafile->_next = datafile->_data + datafile->_currentSize;

      return true;
    }

    ptr += size;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-abc\.ending$/, where abc is
/// a number, and type and ending are arbitrary letters
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetNumericFilenamePart (const char* filename) {
  char const* pos1;
  char const* pos2;

  pos1 = strrchr(filename, '.');

  if (pos1 == nullptr) {
    return 0;
  }

  pos2 = strrchr(filename, '-');

  if (pos2 == nullptr || pos2 > pos1) {
    return 0;
  }

  return TRI_UInt64String2(pos2 + 1, pos1 - pos2 - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the initial datafile header marker
////////////////////////////////////////////////////////////////////////////////

static int WriteInitialHeaderMarker (TRI_datafile_t* datafile,
                                     TRI_voc_fid_t fid,
                                     TRI_voc_size_t maximalSize) {
  // create the header
  TRI_df_header_marker_t header;
  TRI_InitMarkerDatafile((char*) &header, TRI_DF_MARKER_HEADER, sizeof(TRI_df_header_marker_t));
  header.base._tick = (TRI_voc_tick_t) fid;

  header._version     = TRI_DF_VERSION;
  header._maximalSize = maximalSize;
  header._fid         = fid;

  // reserve space and write header to file
  TRI_df_marker_t* position;
  int res = TRI_ReserveElementDatafile(datafile, header.base._size, &position, 0);

  if (res == TRI_ERROR_NO_ERROR) {
    res = TRI_WriteCrcElementDatafile(datafile, position, &header.base, false);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a datafile
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* OpenDatafile (char const* filename,
                                     bool ignoreErrors) {
  TRI_voc_size_t size;
  TRI_voc_fid_t fid;
  bool ok;
  void* data;
  char* ptr;
  int fd;
  int res;
  ssize_t len;
  TRI_stat_t status;
  TRI_df_header_marker_t header;
  void* mmHandle;

  // this function must not be called for non-physical datafiles
  TRI_ASSERT(filename != nullptr);

  fid = GetNumericFilenamePart(filename);

  // ..........................................................................
  // attempt to open a datafile file
  // ..........................................................................

  fd = TRI_OPEN(filename, O_RDWR);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);

    LOG_ERROR("cannot open datafile '%s': '%s'", filename, TRI_last_error());

    return nullptr;
  }

  // compute the size of the file
  res = TRI_FSTAT(fd, &status);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_CLOSE(fd);

    LOG_ERROR("cannot get status of datafile '%s': %s", filename, TRI_last_error());

    return nullptr;
  }

  // check that file is not too small
  size = static_cast<TRI_voc_size_t>(status.st_size);

  if (size < sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t)) {
    TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
    TRI_CLOSE(fd);

    LOG_ERROR("datafile '%s' is corrupt, size is only %u", filename, (unsigned int) size);

    return nullptr;
  }

  // read header from file
  ptr = (char*) &header;
  len = sizeof(TRI_df_header_marker_t);

  ok = TRI_ReadPointer(fd, ptr, len);

  if (! ok) {
    LOG_ERROR("cannot read datafile header from '%s': %s", filename, TRI_last_error());

    TRI_CLOSE(fd);
    return nullptr;
  }

  // check CRC
  ok = CheckCrcMarker(&header.base);

  if (! ok) {
    TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);

    LOG_ERROR("corrupted datafile header read from '%s'", filename);

    if (! ignoreErrors) {
      TRI_CLOSE(fd);
      return nullptr;
    }
  }

  // check the datafile version
  if (ok) {
    if (header._version != TRI_DF_VERSION) {
      TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);

      LOG_ERROR("unknown datafile version '%u' in datafile '%s'",
                (unsigned int) header._version,
                filename);

      if (! ignoreErrors) {
        TRI_CLOSE(fd);
        return nullptr;
      }
    }
  }

  // check the maximal size
  if (size > header._maximalSize) {
    LOG_DEBUG("datafile '%s' has size '%u', but maximal size is '%u'",
              filename,
              (unsigned int) size,
              (unsigned int) header._maximalSize);
  }

  // map datafile into memory
  res = TRI_MMFile(0, size, PROT_READ, MAP_SHARED, fd, &mmHandle, 0, &data);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    TRI_CLOSE(fd);

    LOG_ERROR("cannot memory map datafile '%s': %s", filename, TRI_errno_string(res));
    return nullptr;
  }

  // create datafile structure
  TRI_datafile_t* datafile = static_cast<TRI_datafile_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_datafile_t), false));

  if (datafile == nullptr) {
    TRI_UNMMFile(data, size, fd, &mmHandle);
    TRI_CLOSE(fd);

    return nullptr;
  }

  InitDatafile(datafile,
               TRI_DuplicateString(filename),
               fd,
               mmHandle,
               size,
               size,
               fid,
               static_cast<char*>(data));

  return datafile;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates either an anonymous or a physical datafile
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateDatafile (char const* filename,
                                    TRI_voc_fid_t fid,
                                    TRI_voc_size_t maximalSize,
                                    bool withInitialMarkers) {
  TRI_datafile_t* datafile;

  TRI_ASSERT(PageSize >= 256);

  // use multiples of page-size
  maximalSize = (TRI_voc_size_t) (((maximalSize + PageSize - 1) / PageSize) * PageSize);

  // sanity check maximal size
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) > maximalSize) {
    LOG_ERROR("cannot create datafile, maximal size '%u' is too small", (unsigned int) maximalSize);
    TRI_set_errno(TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL);

    return nullptr;
  }

  // create either an anonymous or a physical datafile
  if (filename == nullptr) {
#ifdef TRI_HAVE_ANONYMOUS_MMAP
    datafile = TRI_CreateAnonymousDatafile(fid, maximalSize);
#else
    // system does not support anonymous mmap
    return nullptr;
#endif
  }
  else {
    datafile = TRI_CreatePhysicalDatafile(filename, fid, maximalSize);
  }

  if (datafile == nullptr) {
    // an error occurred during creation
    return nullptr;
  }


  datafile->_state = TRI_DF_STATE_WRITE;

  if (withInitialMarkers) {
    int res = WriteInitialHeaderMarker(datafile, fid, maximalSize);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("cannot write header to datafile '%s'", datafile->getName(datafile));
      TRI_UNMMFile(datafile->_data, datafile->_maximalSize, datafile->_fd, &datafile->_mmHandle);

      datafile->close(datafile);
      datafile->destroy(datafile);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, datafile);

      return nullptr;
    }
  }

  LOG_DEBUG("created datafile '%s' of size %u and page-size %u",
            datafile->getName(datafile),
            (unsigned int) maximalSize,
            (unsigned int) PageSize);

  return datafile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new anonymous datafile
///
/// this is only supported on certain platforms (Linux, MacOS)
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_ANONYMOUS_MMAP

TRI_datafile_t* TRI_CreateAnonymousDatafile (TRI_voc_fid_t fid,
                                             TRI_voc_size_t maximalSize) {
  TRI_datafile_t* datafile;
  ssize_t res;
  void* data;
  void* mmHandle;
  int flags;
  int fd;

#ifdef TRI_MMAP_ANONYMOUS
  // fd -1 is required for "real" anonymous regions
  fd = -1;
  flags = TRI_MMAP_ANONYMOUS | MAP_SHARED;
#else
  // ugly workaround if MAP_ANONYMOUS is not available
  // TODO: make this more portable
  // TODO: find a good workaround for Windows
  fd = TRI_OPEN("/dev/zero", O_RDWR);
  if (fd == -1) {
    return nullptr;
  }

  flags = MAP_PRIVATE;
#endif

  // memory map the data
  res = TRI_MMFile(nullptr, maximalSize, PROT_WRITE | PROT_READ, flags, fd, &mmHandle, 0, &data);

#ifdef MAP_ANONYMOUS
  // nothing to do
#else
  // close auxilliary file
  TRI_CLOSE(fd);
  fd = -1;
#endif

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    LOG_ERROR("cannot memory map anonymous region: %s", TRI_last_error());
    return nullptr;
  }

  // create datafile structure
  datafile = static_cast<TRI_datafile_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_datafile_t), false));

  if (datafile == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    LOG_ERROR("out of memory");
    return nullptr;
  }

  InitDatafile(datafile,
               nullptr,
               fd,
               mmHandle,
               maximalSize,
               0,
               fid,
               static_cast<char*>(data));

  return datafile;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new physical datafile
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreatePhysicalDatafile (char const* filename,
                                            TRI_voc_fid_t fid,
                                            TRI_voc_size_t maximalSize) {
  TRI_datafile_t* datafile;
  ssize_t res;
  void* data;
  void* mmHandle;

  TRI_ASSERT(filename != nullptr);

  int fd = CreateSparseFile(filename, maximalSize);

  if (fd < 0) {
    // an error occurred
    return nullptr;
  }

  // memory map the data
  res = TRI_MMFile(0, maximalSize, PROT_WRITE | PROT_READ, MAP_SHARED, fd, &mmHandle, 0, &data);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    TRI_CLOSE(fd);

    // remove empty file
    TRI_UnlinkFile(filename);

    LOG_ERROR("cannot memory map file '%s': '%s'", filename, TRI_errno_string((int) res));
    return nullptr;
  }

  // create datafile structure
  datafile = static_cast<TRI_datafile_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_datafile_t), false));

  if (datafile == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    TRI_CLOSE(fd);

    LOG_ERROR("out of memory");
    return nullptr;
  }

  InitDatafile(datafile,
               TRI_DuplicateString(filename),
               fd,
               mmHandle,
               maximalSize,
               0,
               fid,
               static_cast<char*>(data));

  return datafile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDatafile (TRI_datafile_t* datafile) {
  datafile->destroy(datafile);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and but frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeDatafile (TRI_datafile_t* datafile) {
  TRI_DestroyDatafile(datafile);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, datafile);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name for a marker
////////////////////////////////////////////////////////////////////////////////

char const* TRI_NameMarkerDatafile (TRI_df_marker_t const* marker) {
  switch (marker->_type) {
    // general markers
    case TRI_DF_MARKER_HEADER:
    case TRI_COL_MARKER_HEADER:
      return "header";
    case TRI_DF_MARKER_FOOTER:
      return "footer";

    // datafile markers
    case TRI_DOC_MARKER_KEY_DOCUMENT:
      return "document (df)";
    case TRI_DOC_MARKER_KEY_EDGE:
      return "edge (df)";
    case TRI_DOC_MARKER_KEY_DELETION:
      return "deletion (df)";
    case TRI_DOC_MARKER_BEGIN_TRANSACTION:
      return "begin transaction (df)";
    case TRI_DOC_MARKER_COMMIT_TRANSACTION:
      return "commit transaction (df)";
    case TRI_DOC_MARKER_ABORT_TRANSACTION:
      return "abort transaction (df)";
    case TRI_DOC_MARKER_PREPARE_TRANSACTION:
      return "prepare transaction (df)";
    case TRI_DF_MARKER_ATTRIBUTE:
      return "attribute (df)";
    case TRI_DF_MARKER_SHAPE:
      return "shape (df)";

    // wal markers
    case TRI_WAL_MARKER_ATTRIBUTE:
      return "attribute (wal)";
    case TRI_WAL_MARKER_SHAPE:
      return "shape (wal)";
    case TRI_WAL_MARKER_DOCUMENT:
      return "document (wal)";
    case TRI_WAL_MARKER_EDGE:
      return "edge (wal)";
    case TRI_WAL_MARKER_REMOVE:
      return "deletion (wal)";
    case TRI_WAL_MARKER_BEGIN_TRANSACTION:
      return "begin transaction (wal)";
    case TRI_WAL_MARKER_COMMIT_TRANSACTION:
      return "commit transaction (wal)";
    case TRI_WAL_MARKER_ABORT_TRANSACTION:
      return "abort transaction (wal)";
    case TRI_WAL_MARKER_BEGIN_REMOTE_TRANSACTION:
      return "begin remote transaction (wal)";
    case TRI_WAL_MARKER_COMMIT_REMOTE_TRANSACTION:
      return "commit remote transaction (wal)";
    case TRI_WAL_MARKER_ABORT_REMOTE_TRANSACTION:
      return "abort remote transaction (wal)";
    case TRI_WAL_MARKER_CREATE_COLLECTION:
      return "create collection (wal)";
    case TRI_WAL_MARKER_DROP_COLLECTION:
      return "drop collection (wal)";
    case TRI_WAL_MARKER_RENAME_COLLECTION:
      return "rename collection (wal)";
    case TRI_WAL_MARKER_CHANGE_COLLECTION:
      return "change collection (wal)";
    case TRI_WAL_MARKER_CREATE_INDEX:
      return "create index (wal)";
    case TRI_WAL_MARKER_DROP_INDEX:
      return "drop index (wal)";
    case TRI_WAL_MARKER_CREATE_DATABASE:
      return "create database (wal)";
    case TRI_WAL_MARKER_DROP_DATABASE:
      return "drop database (wal)";

    default:
      return "unused/unknown";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a marker with the most basic information
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMarkerDatafile (char* marker,
                             TRI_df_marker_type_e type,
                             TRI_voc_size_t size) {

  TRI_ASSERT(marker != nullptr);
  TRI_ASSERT(type > TRI_MARKER_MIN && type < TRI_MARKER_MAX);
  TRI_ASSERT(size > 0);

  // initialise the basic bytes
  memset(marker, 0, size);

  TRI_df_marker_t* df = reinterpret_cast<TRI_df_marker_t*>(marker);
  df->_size = size;
  df->_type = type;
  // not needed because of memset above
  // marker->_crc  = 0;
  // marker->_tick = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a marker is valid
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsValidMarkerDatafile (TRI_df_marker_t const* marker) {
  if (marker == nullptr) {
    return false;
  }

  // check marker type
  TRI_df_marker_type_t type = marker->_type;
  if (type <= (TRI_df_marker_type_t) TRI_MARKER_MIN) {
    // marker type is less than minimum allowed type value
    return false;
  }

  if (type >= (TRI_df_marker_type_t) TRI_MARKER_MAX) {
    // marker type is greater than maximum allowed type value
    return false;
  }

  if (marker->_size >= (TRI_voc_size_t) TRI_MARKER_MAXIMAL_SIZE) {
    // a single marker bigger than 256 MB seems unreasonable
    // note: this is an arbitrary limit
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserves room for an element, advances the pointer
///
/// note: maximalJournalSize is the collection's maximalJournalSize property,
/// which may be different from the size of the current datafile
/// some callers do not set the value of maximalJournalSize
////////////////////////////////////////////////////////////////////////////////

int TRI_ReserveElementDatafile (TRI_datafile_t* datafile,
                                TRI_voc_size_t size,
                                TRI_df_marker_t** position,
                                TRI_voc_size_t maximalJournalSize) {
  *position = 0;
  size = TRI_DF_ALIGN_BLOCK(size);

  if (datafile->_state != TRI_DF_STATE_WRITE) {
    if (datafile->_state == TRI_DF_STATE_READ) {
      LOG_ERROR("cannot reserve marker, datafile is read-only");

      return TRI_set_errno(TRI_ERROR_ARANGO_READ_ONLY);
    }

    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
  }

  // check the maximal size
  if (size + TRI_JOURNAL_OVERHEAD > datafile->_maximalSize) {
    // marker is bigger than journal size.
    // adding the marker to this datafile will not work

    if (maximalJournalSize <= datafile->_maximalSize) {
      // the collection property 'maximalJournalSize' is equal to
      // or smaller than the size of this datafile
      // creating a new file and writing the marker into it will not work either
      return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
    }

    // if we get here, the collection's 'maximalJournalSize' property is
    // higher than the size of this datafile.
    // maybe the marker will fit into a new datafile with the bigger size?
    if (size + TRI_JOURNAL_OVERHEAD > maximalJournalSize) {
      // marker still won't fit
      return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
    }

    // fall-through intentional
  }

  // add the marker, leave enough room for the footer
  if (datafile->_currentSize + size + datafile->_footerSize > datafile->_maximalSize) {
    datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_FULL);
    datafile->_full = true;

    LOG_TRACE("cannot write marker, not enough space");

    return datafile->_lastError;
  }

  *position = (TRI_df_marker_t*) datafile->_next;

  datafile->_next += size;
  datafile->_currentSize += size;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a marker to the datafile
/// this function will write the marker as-is, without any CRC or tick updates
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteElementDatafile (TRI_datafile_t* datafile,
                              void* position,
                              TRI_df_marker_t const* marker,
                              bool forceSync) {
  TRI_ASSERT(marker->_tick > 0);
  TRI_ASSERT(marker->_size > 0);

  TRI_UpdateTicksDatafile(datafile, marker);

  if (datafile->_state != TRI_DF_STATE_WRITE) {
    if (datafile->_state == TRI_DF_STATE_READ) {
      LOG_ERROR("cannot write marker, datafile is read-only");

      return TRI_set_errno(TRI_ERROR_ARANGO_READ_ONLY);
    }

    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
  }

  // out of bounds check for writing into a datafile
  if (position < (void*) datafile->_data ||
      position >= (void*) (datafile->_data + datafile->_maximalSize)) {

    LOG_ERROR("logic error. writing out of bounds of datafile '%s'", datafile->getName(datafile));
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
  }

  memcpy(position, marker, static_cast<size_t>(marker->_size));


  if (forceSync) {
    bool ok = datafile->sync(datafile, static_cast<char const*>(position), ((char*) position) + marker->_size);

    if (! ok) {
      datafile->_state = TRI_DF_STATE_WRITE_ERROR;

      if (errno == ENOSPC) {
        datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
      }
      else {
        datafile->_lastError = TRI_set_errno(TRI_ERROR_SYS_ERROR);
      }

      LOG_ERROR("msync failed with: %s", TRI_last_error());

      return datafile->_lastError;
    }
    else {
      LOG_TRACE("msync succeeded %p, size %lu", position, (unsigned long) marker->_size);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update tick values for a datafile
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTicksDatafile (TRI_datafile_t* datafile,
                              TRI_df_marker_t const* marker) {
  TRI_df_marker_type_e type = (TRI_df_marker_type_e) marker->_type;
  TRI_voc_tick_t tick = marker->_tick;
  
  if (type != TRI_DF_MARKER_HEADER && 
      type != TRI_DF_MARKER_FOOTER &&
      type != TRI_COL_MARKER_HEADER) {
    // every marker but headers / footers count
    if (datafile->_tickMin == 0) {
      datafile->_tickMin = tick;
    }

    if (datafile->_tickMax < marker->_tick) {
      datafile->_tickMax = tick;
    }

    if (type != TRI_DF_MARKER_ATTRIBUTE &&
        type != TRI_DF_MARKER_SHAPE &&
        type != TRI_WAL_MARKER_ATTRIBUTE &&
        type != TRI_WAL_MARKER_SHAPE) {
      if (datafile->_dataMin == 0) {
        datafile->_dataMin = tick;
      }

      if (datafile->_dataMax < tick) {
        datafile->_dataMax = tick;
      }
    }
  }

// TODO: check whether the following code can be removed safely
/*
  if (type != TRI_DF_MARKER_HEADER &&
      type != TRI_DF_MARKER_FOOTER &&
      type != TRI_COL_MARKER_HEADER &&
      type != TRI_DF_MARKER_ATTRIBUTE &&
      type != TRI_DF_MARKER_SHAPE) {

#ifdef TRI_ENABLE_MAINTAINER_MODE
    // check _tick value of marker and set min/max tick values for datafile
    if (marker->_tick < datafile->_tickMin) {
      LOG_FATAL_AND_EXIT("logic error. invalid tick value %llu encountered when writing marker of type %d into datafile '%s'. "
          "expected tick value >= tickMin %llu",
          (unsigned long long) tick,
          (int) marker->_type,
          datafile->getName(datafile),
          (unsigned long long) datafile->_tickMin);
    }

    if (tick < datafile->_tickMax) {
      LOG_FATAL_AND_EXIT("logic error. invalid tick value %llu encountered when writing marker of type %d into datafile '%s'. "
          "expected tick value >= tickMax %llu",
          (unsigned long long) tick,
          (int) marker->_type,
          datafile->getName(datafile),
          (unsigned long long) datafile->_tickMax);
    }

    if (tick < static_cast<TRI_voc_tick_t>(datafile->_fid)) {
      LOG_FATAL_AND_EXIT("logic error. invalid tick value %llu encountered when writing marker of type %d into datafile '%s'. "
          "expected tick value >= fid %llu",
          (unsigned long long) tick,
          (int) marker->_type,
          datafile->getName(datafile),
          (unsigned long long) datafile->_fid);
    }
#endif

    if (type == TRI_DOC_MARKER_KEY_DOCUMENT ||
        type == TRI_DOC_MARKER_KEY_EDGE) {
      if (datafile->_dataMin == 0) {
        datafile->_dataMin = tick;
      }

      if (datafile->_dataMax < tick) {
        datafile->_dataMax = tick;
      }
    }
  }

  if (type != TRI_DF_MARKER_ATTRIBUTE &&
      type != TRI_DF_MARKER_SHAPE) {

    if (datafile->_tickMin == 0) {
      datafile->_tickMin = tick;
    }

    if (datafile->_tickMax < marker->_tick) {
      datafile->_tickMax = tick;
    }
  }
  */
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checksums and writes a marker to the datafile
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteCrcElementDatafile (TRI_datafile_t* datafile,
                                 void* position,
                                 TRI_df_marker_t* marker,
                                 bool forceSync) {
  TRI_ASSERT(marker->_tick != 0);

  if (datafile->isPhysical(datafile)) {
    TRI_voc_crc_t crc = TRI_InitialCrc32();

    crc = TRI_BlockCrc32(crc, (char const*) marker, marker->_size);
    marker->_crc = TRI_FinalCrc32(crc);
  }

  return TRI_WriteElementDatafile(datafile, position, marker, forceSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a datafile
/// also may set datafile's min/max tick values
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateDatafile (TRI_datafile_t* datafile,
                          bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*),
                          void* data) {
  LOG_TRACE("iterating over datafile '%s', fid: %llu",
            datafile->getName(datafile),
            (unsigned long long) datafile->_fid);

  char const* ptr = datafile->_data;
  char const* end = datafile->_data + datafile->_currentSize;

  if (datafile->_state != TRI_DF_STATE_READ && datafile->_state != TRI_DF_STATE_WRITE) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
    return false;
  }

  while (ptr < end) {
    TRI_df_marker_t const* marker = reinterpret_cast<TRI_df_marker_t const*>(ptr);
    if (marker->_size == 0) {
      return true;
    }

    // update the tick statistics
    TRI_UpdateTicksDatafile(datafile, marker);

    if (iterator != nullptr) {
      bool result = iterator(marker, data, datafile);

      if (! result) {
        return false;
      }
    }

    size_t size = TRI_DF_ALIGN_BLOCK(marker->_size);
    ptr += size;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing datafile
///
/// The datafile will be opened read-only if a footer is found
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_OpenDatafile (char const* filename,
                                  bool ignoreFailures) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(filename != nullptr);

  TRI_datafile_t* datafile = OpenDatafile(filename, false);

  if (datafile == nullptr) {
    return nullptr;
  }

  // check the datafile by scanning markers
  bool ok = CheckDatafile(datafile, ignoreFailures);

  if (! ok) {
    TRI_UNMMFile(datafile->_data, datafile->_maximalSize, datafile->_fd, &datafile->_mmHandle);
    TRI_CLOSE(datafile->_fd);

    LOG_ERROR("datafile '%s' is corrupt", datafile->getName(datafile));
    // must free datafile here
    TRI_FreeDatafile(datafile);

    return nullptr;
  }

  // change to read-write if no footer has been found
  if (! datafile->_isSealed) {
    datafile->_state = TRI_DF_STATE_WRITE;
    TRI_ProtectMMFile(datafile->_data, datafile->_maximalSize, PROT_READ | PROT_WRITE, datafile->_fd, &datafile->_mmHandle);
  }

  return datafile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing, possibly corrupt datafile
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_ForcedOpenDatafile (char const* filename) {
  TRI_datafile_t* datafile;
  bool ok;

  // this function must not be called for non-physical datafiles
  TRI_ASSERT(filename != nullptr);

  datafile = OpenDatafile(filename, true);

  if (datafile == nullptr) {
    return nullptr;
  }

  // check the current marker
  ok = CheckDatafile(datafile, true);

  if (! ok) {
    LOG_ERROR("datafile '%s' is corrupt", datafile->getName(datafile));
  }

  // change to read-write if no footer has been found
  else {
    if (! datafile->_isSealed) {
      datafile->_state = TRI_DF_STATE_WRITE;
      TRI_ProtectMMFile(datafile->_data, datafile->_maximalSize, PROT_READ | PROT_WRITE, datafile->_fd, &datafile->_mmHandle);
    }
  }

  return datafile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a datafile and all memory regions
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseDatafile (TRI_datafile_t* datafile) {
  if (datafile->_state == TRI_DF_STATE_READ || datafile->_state == TRI_DF_STATE_WRITE) {
    int res;

    res = TRI_UNMMFile(datafile->_data, datafile->_maximalSize, datafile->_fd, &datafile->_mmHandle);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("munmap failed with: %d", res);
      datafile->_state = TRI_DF_STATE_WRITE_ERROR;
      datafile->_lastError = res;
      return false;
    }

    else {
      datafile->close(datafile);
      datafile->_data = 0;
      datafile->_next = 0;
      datafile->_fd = -1;

      return true;
    }
  }
  else if (datafile->_state == TRI_DF_STATE_CLOSED) {
    LOG_WARNING("closing an already closed datafile '%s'", datafile->getName(datafile));
    return true;
  }
  else {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a datafile
////////////////////////////////////////////////////////////////////////////////

bool TRI_RenameDatafile (TRI_datafile_t* datafile, char const* filename) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(datafile->isPhysical(datafile));
  TRI_ASSERT(filename != nullptr);

  if (TRI_ExistsFile(filename)) {
    LOG_ERROR("cannot overwrite datafile '%s'", filename);

    datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS);
    return false;
  }

  int res = TRI_RenameFile(datafile->_filename, filename);

  if (res != TRI_ERROR_NO_ERROR) {
    datafile->_state = TRI_DF_STATE_RENAME_ERROR;
    datafile->_lastError = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    return false;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, datafile->_filename);
  datafile->_filename = TRI_DuplicateString(filename);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief seals a datafile, writes a footer, sets it to read-only
////////////////////////////////////////////////////////////////////////////////

int TRI_SealDatafile (TRI_datafile_t* datafile) {
  TRI_df_footer_marker_t footer;
  TRI_df_marker_t* position;

  if (datafile->_state == TRI_DF_STATE_READ) {
    return TRI_set_errno(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (datafile->_state != TRI_DF_STATE_WRITE) {
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
  }

  if (datafile->_isSealed) {
    return TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_SEALED);
  }


  // create the footer
  TRI_InitMarkerDatafile((char*) &footer, TRI_DF_MARKER_FOOTER, sizeof(TRI_df_footer_marker_t));
  // set a proper tick value
  footer.base._tick = datafile->_tickMax;

  // reserve space and write footer to file
  datafile->_footerSize = 0;

  int res = TRI_ReserveElementDatafile(datafile, footer.base._size, &position, 0);

  if (res == TRI_ERROR_NO_ERROR) {
    res = TRI_WriteCrcElementDatafile(datafile, position, &footer.base, false);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // sync file
  bool ok = datafile->sync(datafile, datafile->_synced, ((char*) datafile->_data) + datafile->_currentSize);

  if (! ok) {
    datafile->_state = TRI_DF_STATE_WRITE_ERROR;

    if (errno == ENOSPC) {
      datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
    }
    else {
      datafile->_lastError = TRI_errno();
    }

    LOG_ERROR("msync failed with: %s", TRI_last_error());
  }

  // everything is now synced
  datafile->_synced = datafile->_written;

  /*
    TODO: do we have to unmap file? That is, release the memory which has been allocated for
          this file? At the moment the windows of function TRI_ProtectMMFile does nothing.
  */
  TRI_ProtectMMFile(datafile->_data, datafile->_maximalSize, PROT_READ, datafile->_fd, &datafile->_mmHandle);

  // truncate datafile
  if (ok) {
    #ifdef _WIN32
      res = 0;
      /*
      res = ftruncate(datafile->_fd, datafile->_currentSize);
      Linux centric problems:
        Under windows can not reduce size of the memory mapped file without unmapping it!
        However, apparently we may have users
      */
    #else
      res = datafile->truncate(datafile, datafile->_currentSize);
    #endif

    if (res < 0) {
      LOG_ERROR("cannot truncate datafile '%s': %s", datafile->getName(datafile), TRI_last_error());
      datafile->_lastError = TRI_set_errno(TRI_ERROR_SYS_ERROR);
      datafile->_state = TRI_DF_STATE_WRITE_ERROR;
      ok = false;
    }

    datafile->_isSealed = true;
    datafile->_state = TRI_DF_STATE_READ;
    datafile->_maximalSize = datafile->_currentSize;
  }

  if (! ok) {
    return datafile->_lastError;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a datafile and seals it
/// this is called from the recovery procedure only
////////////////////////////////////////////////////////////////////////////////

int TRI_TruncateDatafile (char const* path,
                          TRI_voc_size_t position) {
  // this function must not be called for non-physical datafiles
  TRI_ASSERT(path != nullptr);

  TRI_datafile_t* datafile = OpenDatafile(path, true);

  if (datafile == nullptr) {
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  int res = TruncateAndSealDatafile(datafile, position);
  TRI_CloseDatafile(datafile);
  TRI_FreeDatafile(datafile);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the datafile
////////////////////////////////////////////////////////////////////////////////

TRI_df_scan_t TRI_ScanDatafile (char const* path) {
  TRI_df_scan_t scan;

  // this function must not be called for non-physical datafiles
  TRI_ASSERT(path != nullptr);

  TRI_datafile_t* datafile = OpenDatafile(path, true);

  if (datafile != nullptr) {
    scan = ScanDatafile(datafile);
    TRI_CloseDatafile(datafile);
    TRI_FreeDatafile(datafile);
  }
  else {
    scan._currentSize = 0;
    scan._maximalSize = 0;
    scan._endPosition = 0;
    scan._numberMarkers = 0;

    TRI_InitVector(&scan._entries, TRI_CORE_MEM_ZONE, sizeof(TRI_df_scan_entry_t));

    scan._status   = 5;
    scan._isSealed = false;
  }

  return scan;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys information about the datafile
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDatafileScan (TRI_df_scan_t* scan) {
  TRI_DestroyVector(&scan->_entries);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
