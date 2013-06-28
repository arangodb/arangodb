////////////////////////////////////////////////////////////////////////////////
/// @brief datafiles
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "datafile.h"

#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "BasicsC/memory-map.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/files.h"

#include "VocBase/marker.h"


// #define DEBUG_DATAFILE 1

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the datafile is a physical file (true) or an
/// anonymous mapped region (false)
////////////////////////////////////////////////////////////////////////////////

static bool IsPhysicalDatafile (const TRI_datafile_t* const datafile) {
  return datafile->_filename != NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of a datafile
////////////////////////////////////////////////////////////////////////////////

static const char* GetNameDatafile (const TRI_datafile_t* const datafile) {
  if (datafile->_filename == NULL) {
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
  assert(datafile->_state != TRI_DF_STATE_CLOSED);

  if (datafile->isPhysical(datafile)) {
    TRI_CLOSE(datafile->_fd);
  }

  datafile->_state = TRI_DF_STATE_CLOSED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a datafile
////////////////////////////////////////////////////////////////////////////////

static void DestroyDatafile (TRI_datafile_t* const datafile) {
  if (datafile->_filename != NULL) {
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
  void** mmHandle = NULL;

  if (datafile->_filename == NULL) {
    // anonymous regions do not need to be synced
    return true;
  }

  assert(datafile->_fd > 0);

  return TRI_msync(datafile->_fd, mmHandle, begin, end);
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
/// @brief creates a new sparse datafile
///
/// returns the file descriptor or -1 if the file cannot be created
////////////////////////////////////////////////////////////////////////////////

static int CreateSparseFile (char const* filename,
                             const TRI_voc_size_t maximalSize) {
  off_t offset;
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
  offset = TRI_LSEEK(fd, maximalSize - 1, SEEK_SET);

  if (offset == (off_t) -1) {
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
  if (filename == NULL) {
    assert(fd == -1);
  }
  else {
    assert(fd > 0);
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
  datafile->_written     = NULL;
  
  datafile->_tickMin     = 0;
  datafile->_tickMax     = 0;

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
  char* filename;
  char* oldname;
  char zero;
  int fd;
  int res;
  size_t maximalSize;
  off_t offset;
  void* data;
  void* mmHandle;

  // this function must not be called for non-physical datafiles
  assert(datafile->isPhysical(datafile));

  // use multiples of page-size
  maximalSize = ((vocSize + sizeof(TRI_df_footer_marker_t) + PageSize - 1) / PageSize) * PageSize;

  // sanity check
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) > maximalSize) {
    LOG_ERROR("cannot create datafile '%s', maximal size '%u' is too small", datafile->getName(datafile), (unsigned int) maximalSize);
    return TRI_set_errno(TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL);
  }

  // open the file
  filename = TRI_Concatenate2String(datafile->_filename, ".new");

  fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    LOG_ERROR("cannot create new datafile '%s': '%s'", filename, TRI_last_error());
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  // create sparse file
  offset = TRI_LSEEK(fd, maximalSize - 1, SEEK_SET);

  if (offset == (off_t) -1) {
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
  res = TRI_UNMMFile(datafile->_data, datafile->_maximalSize, datafile->_fd, &(datafile->_mmHandle));

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


  datafile->_data = data;
  datafile->_next = (char*)(data) + vocSize;
  datafile->_maximalSize = maximalSize;
  datafile->_fd = fd;
  datafile->_mmHandle = mmHandle;

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

  TRI_SealDatafile(datafile);
  return TRI_ERROR_NO_ERROR;
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
  assert(datafile->isPhysical(datafile));

  ptr = datafile->_data;
  end = datafile->_data + datafile->_currentSize;
  currentSize = 0;

  TRI_InitVector(&scan._entries, TRI_CORE_MEM_ZONE, sizeof(TRI_df_scan_entry_t));

  scan._currentSize = datafile->_currentSize;
  scan._maximalSize = datafile->_maximalSize;
  scan._numberMarkers = 0;
  scan._status = 1;

  if (datafile->_currentSize == 0) {
    end = datafile->_data + datafile->_maximalSize;
  }

  while (ptr < end) {
    TRI_df_marker_t* marker = (TRI_df_marker_t*) ptr;
    bool ok;
    size_t size;

    memset(&entry, 0, sizeof(entry));

    entry._position = ptr - datafile->_data;
    entry._size = marker->_size;
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

    ok = TRI_CheckCrcMarkerDatafile(marker);

    if (! ok) {
      entry._status = 5;
      scan._status = 4;
    }

    TRI_PushBackVector(&scan._entries, &entry);

    size = TRI_DF_ALIGN_BLOCK(marker->_size);
    currentSize += size;

    if (marker->_type == TRI_DF_MARKER_FOOTER) {
      scan._endPosition = currentSize;
      return scan;
    }

    ptr += size;
  }

  return scan;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile
////////////////////////////////////////////////////////////////////////////////

static bool CheckDatafile (TRI_datafile_t* datafile) {
  TRI_voc_size_t currentSize;
  char* end;
  char* ptr;

  // this function must not be called for non-physical datafiles
  assert(datafile->isPhysical(datafile));

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

    if (marker->_type != 0) {
      bool ok;

      ok = TRI_CheckCrcMarkerDatafile(marker);

      if (! ok) {
        datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
        datafile->_currentSize = currentSize;
        datafile->_next = datafile->_data + datafile->_currentSize;
        datafile->_state = TRI_DF_STATE_OPEN_ERROR;

        LOG_WARNING("crc mismatch found in datafile '%s'", datafile->getName(datafile));

        return false;
      }

      TRI_UpdateTickVocBase(marker->_tick);
    }

    size = TRI_DF_ALIGN_BLOCK(marker->_size);
    currentSize += size;

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
/// @brief opens a datafile
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* OpenDatafile (char const* filename, bool ignoreErrors) {
  TRI_datafile_t* datafile;
  TRI_voc_size_t size;
  bool ok;
  void* data;
  char* ptr;
  int fd;
  int res;
  ssize_t len;
  struct stat status;
  TRI_df_header_marker_t header;
  void* mmHandle;

  // this function must not be called for non-physical datafiles
  assert(filename != NULL);

  // ..........................................................................
  // attempt to open a datafile file
  // ..........................................................................

  fd = TRI_OPEN(filename, O_RDWR);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);

    LOG_ERROR("cannot open datafile '%s': '%s'", filename, TRI_last_error());

    return NULL;
  }


  // compute the size of the file
  res = fstat(fd, &status);

  if (res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_CLOSE(fd);

    LOG_ERROR("cannot get status of datafile '%s': %s", filename, TRI_last_error());

    return NULL;
  }

  // check that file is not too small
  size = status.st_size;

  if (size < sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t)) {
    TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
    TRI_CLOSE(fd);

    LOG_ERROR("datafile '%s' is corrupt, size is only %u", filename, (unsigned int) size);

    return NULL;
  }

  // read header from file
  ptr = (char*) &header;
  len = sizeof(TRI_df_header_marker_t);

  ok = TRI_ReadPointer(fd, ptr, len);

  if (! ok) {
    LOG_ERROR("cannot read datafile header from '%s': %s", filename, TRI_last_error());

    TRI_CLOSE(fd);
    return NULL;
  }

  // check CRC
  ok = TRI_CheckCrcMarkerDatafile(&header.base);

  if (! ok) {
    TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);

    LOG_ERROR("corrupted datafile header read from '%s'", filename);

    if (! ignoreErrors) {
      TRI_CLOSE(fd);
      return NULL;
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
        return NULL;
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

    LOG_ERROR("cannot memory map datafile '%s': '%d'", filename, res);
    return NULL;
  }

  // create datafile structure
  datafile = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_datafile_t), false);

  if (datafile == NULL) {
    TRI_UNMMFile(data, size, fd, &mmHandle);
    TRI_CLOSE(fd);

    return NULL;
  }

  InitDatafile(datafile,
               TRI_DuplicateString(filename),
               fd,
               mmHandle,
               size,
               size,
               header._fid,
               data);

  return datafile;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates either an anonymous or a physical datafile
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateDatafile (char const* filename,
                                    TRI_voc_fid_t fid,
                                    TRI_voc_size_t maximalSize) {
  TRI_datafile_t* datafile;
  TRI_df_marker_t* position;
  TRI_df_header_marker_t header;
  int result;

  // use multiples of page-size
  maximalSize = ((maximalSize + PageSize - 1) / PageSize) * PageSize;

  // sanity check maximal size
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) > maximalSize) {
    LOG_ERROR("cannot create datafile, maximal size '%u' is too small", (unsigned int) maximalSize);
    TRI_set_errno(TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL);

    return NULL;
  }

  // create either an anonymous or a physical datafile
  if (filename == NULL) {
#ifdef TRI_HAVE_ANONYMOUS_MMAP
    datafile = TRI_CreateAnonymousDatafile(fid, maximalSize);
#else
    // system does not support anonymous mmap
    return NULL;
#endif
  }
  else {
    datafile = TRI_CreatePhysicalDatafile(filename, fid, maximalSize);
  }

  if (datafile == NULL) {
    // an error occurred during creation
    return NULL;
  }


  datafile->_state = TRI_DF_STATE_WRITE;

  // create the header
  TRI_InitMarker(&header.base, TRI_DF_MARKER_HEADER, sizeof(TRI_df_header_marker_t));
  header.base._tick = (TRI_voc_tick_t) fid;

  header._version     = TRI_DF_VERSION;
  header._maximalSize = maximalSize;
  header._fid         = fid;

  // reserve space and write header to file
  result = TRI_ReserveElementDatafile(datafile, header.base._size, &position, 0);

  if (result == TRI_ERROR_NO_ERROR) {
    result = TRI_WriteCrcElementDatafile(datafile, position, &header.base, header.base._size, true);
  }

  if (result != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot write header to datafile '%s'", datafile->getName(datafile));
    TRI_UNMMFile(datafile->_data, datafile->_maximalSize, datafile->_fd, &(datafile->_mmHandle));

    datafile->close(datafile);
    datafile->destroy(datafile);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, datafile);

    return NULL;
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
                                             const TRI_voc_size_t maximalSize) {
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
    return NULL;
  }

  flags = MAP_PRIVATE;
#endif

  // memory map the data
  res = TRI_MMFile(NULL, maximalSize, PROT_WRITE | PROT_READ, flags, fd, &mmHandle, 0, &data);

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
    return NULL;
  }

  // create datafile structure
  datafile = (TRI_datafile_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_datafile_t), false);

  if (datafile == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    LOG_ERROR("out of memory");
    return NULL;
  }

  InitDatafile(datafile,
               NULL,
               fd,
               mmHandle,
               maximalSize,
               0,
               fid,
               data);

  return datafile;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new physical datafile
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreatePhysicalDatafile (char const* filename, 
                                            TRI_voc_fid_t fid,
                                            const TRI_voc_size_t maximalSize) {
  TRI_datafile_t* datafile;
  int fd;
  ssize_t res;
  void* data;
  void* mmHandle;

  assert(filename != NULL);

  fd = CreateSparseFile(filename, maximalSize);
  if (fd <= 0) {
    // an error occurred
    return NULL;
  }

  // memory map the data
  res = TRI_MMFile(0, maximalSize, PROT_WRITE | PROT_READ, MAP_SHARED, fd, &mmHandle, 0, &data);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    TRI_CLOSE(fd);

    // remove empty file
    TRI_UnlinkFile(filename);

    LOG_ERROR("cannot memory map file '%s': '%s'", filename, TRI_errno_string((int) res));
    return NULL;
  }


  // create datafile structure
  datafile = (TRI_datafile_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_datafile_t), false);

  if (datafile == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    TRI_CLOSE(fd);

    LOG_ERROR("out of memory");
    return NULL;
  }

  InitDatafile(datafile,
               TRI_DuplicateString(filename),
               fd,
               mmHandle,
               maximalSize,
               0,
               fid,
               data);

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a marker is valid
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsValidMarkerDatafile (TRI_df_marker_t* const marker) {
  TRI_df_marker_type_t type;

  if (marker == 0) {
    return false;
  }

  // check marker type
  type = marker->_type;
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
/// @brief checks a CRC of a marker
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckCrcMarkerDatafile (TRI_df_marker_t const* marker) {
  TRI_voc_size_t zero;
  char const* ptr;
  off_t o;
  size_t n;
  TRI_voc_crc_t crc;

  zero = 0;
  o = offsetof(TRI_df_marker_t, _crc);
  n = sizeof(TRI_voc_crc_t);

  ptr = (char const*) marker;

  if (marker->_size < sizeof(TRI_df_marker_t)) {
    return false;
  }

  crc = TRI_InitialCrc32();

  crc = TRI_BlockCrc32(crc, ptr, o);
  crc = TRI_BlockCrc32(crc, (char*) &zero, n);
  crc = TRI_BlockCrc32(crc, ptr + o + n, marker->_size - o - n);

  crc = TRI_FinalCrc32(crc);

  return marker->_crc == crc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a CRC and writes that into the header
/// @deprecated this function is deprecated. do not use for new code
////////////////////////////////////////////////////////////////////////////////

void TRI_FillCrcKeyMarkerDatafile (TRI_datafile_t* datafile,
                                   TRI_df_marker_t* marker,
                                   TRI_voc_size_t markerSize,
                                   void const* keyBody,
                                   TRI_voc_size_t keyBodySize,
                                   void const* body,
                                   TRI_voc_size_t bodySize) {
  marker->_crc = 0;

  // crc values only need to be generated for physical files
  if (datafile->isPhysical(datafile)) {
    TRI_voc_crc_t crc;

    crc = TRI_InitialCrc32();
    crc = TRI_BlockCrc32(crc, (char const*) marker, markerSize);

    if (keyBody != NULL && 0 < keyBodySize) {
      crc = TRI_BlockCrc32(crc, keyBody, keyBodySize);
    }

    if (body != NULL && 0 < bodySize) {
      crc = TRI_BlockCrc32(crc, body, bodySize);
    }

    marker->_crc = TRI_FinalCrc32(crc);
  }
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
                              TRI_voc_size_t markerSize,
                              bool forceSync) {
  TRI_voc_tick_t tick = marker->_tick;
   
  assert(tick > 0);

  if (marker->_type != TRI_DF_MARKER_HEADER && 
      marker->_type != TRI_DF_MARKER_FOOTER &&
      marker->_type != TRI_COL_MARKER_HEADER) {

    // check _tick value of marker and set min/max values for datafile
    if (tick <= datafile->_tickMin || tick <= (TRI_voc_tick_t) datafile->_fid) {
      LOG_WARNING("logic error. invalid tick value %llu encountered when writing marker of type %d into datafile '%s'. "
          "expected tick value > tickMin %llu",
          (unsigned long long) tick,
          (int) marker->_type,
          datafile->getName(datafile),
          (unsigned long long) datafile->_tickMin);
    }

    if (tick <= datafile->_tickMax) {
      LOG_WARNING("logic error. invalid tick value %llu encountered when writing marker of type %d into datafile '%s'. "
          "expected tick value > tickMax %llu",
          (unsigned long long) tick,
          (int) marker->_type,
          datafile->getName(datafile),
          (unsigned long long) datafile->_tickMax);
    }
  }

  if (datafile->_tickMin == 0) {
    datafile->_tickMin = tick;
  }
  datafile->_tickMax = tick;
   
  assert(markerSize > 0);

  if (markerSize != marker->_size) {
    LOG_ERROR("marker size is %lu, but size is %lu",
              (unsigned long) marker->_size,
              (unsigned long) markerSize);
  }

  if (datafile->_state != TRI_DF_STATE_WRITE) {
    if (datafile->_state == TRI_DF_STATE_READ) {
      LOG_ERROR("cannot write marker, datafile is read-only");

      return TRI_set_errno(TRI_ERROR_ARANGO_READ_ONLY);
    }

    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
  }

  memcpy(position, marker, markerSize);


  if (forceSync) {
    bool ok;

    ok = datafile->sync(datafile, position, ((char*) position) + markerSize);

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
      LOG_TRACE("msync succeeded %p, size %lu", position, (unsigned long) markerSize);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checksums and writes a marker to the datafile
/// this function will also assign a new tick value for the marker (so that
/// the tick values are increasing)
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteCrcElementDatafile (TRI_datafile_t* datafile,
                                 void* position,
                                 TRI_df_marker_t* marker,
                                 TRI_voc_size_t markerSize,
                                 bool forceSync) {
  if (marker->_tick == 0) {
    // set a tick value for the marker
    marker->_tick = TRI_NewTickVocBase();
  }

  if (datafile->isPhysical(datafile)) {
    TRI_voc_crc_t crc = TRI_InitialCrc32();
  
    crc = TRI_BlockCrc32(crc, (char const*) marker, markerSize);
    marker->_crc = TRI_FinalCrc32(crc);
  }

  return TRI_WriteElementDatafile(datafile, position, marker, markerSize, forceSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a datafile
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateDatafile (TRI_datafile_t* datafile,
                          bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*, bool),
                          void* data,
                          bool journal) {
  char* ptr;
  char* end;

  LOG_TRACE("iterating over datafile '%s', fid: %llu", datafile->getName(datafile), (unsigned long long) datafile->_fid);

  ptr = datafile->_data;
  end = datafile->_data + datafile->_currentSize;

  if (datafile->_state != TRI_DF_STATE_READ && datafile->_state != TRI_DF_STATE_WRITE) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);
    return false;
  }

  while (ptr < end) {
    TRI_df_marker_t* marker = (TRI_df_marker_t*) ptr;
    bool result;
    size_t size;

    if (marker->_size == 0) {
      return true;
    }

    result = iterator(marker, data, datafile, journal);

    if (! result) {
      return false;
    }

    size = TRI_DF_ALIGN_BLOCK(marker->_size);
    ptr += size;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing datafile
///
/// The datafile will be opened read-only if a footer is found
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_OpenDatafile (char const* filename) {
  TRI_datafile_t* datafile;
  bool ok;

  // this function must not be called for non-physical datafiles
  assert(filename != NULL);

  datafile = OpenDatafile(filename, false);

  if (datafile == NULL) {
    return NULL;
  }

  // check the current marker
  ok = CheckDatafile(datafile);

  if (! ok) {
    TRI_UNMMFile(datafile->_data, datafile->_maximalSize, datafile->_fd, &(datafile->_mmHandle));
    TRI_CLOSE(datafile->_fd);

    LOG_ERROR("datafile '%s' is corrupt", datafile->getName(datafile));
    // must free datafile here
    TRI_FreeDatafile(datafile);

    return NULL;
  }

  // change to read-write if no footer has been found
  if (! datafile->_isSealed) {
    datafile->_state = TRI_DF_STATE_WRITE;
    TRI_ProtectMMFile(datafile->_data, datafile->_maximalSize, PROT_READ | PROT_WRITE, datafile->_fd, &(datafile->_mmHandle));
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
  assert(filename != NULL);

  datafile = OpenDatafile(filename, true);

  if (datafile == NULL) {
    return NULL;
  }

  // check the current marker
  ok = CheckDatafile(datafile);

  if (! ok) {
    LOG_ERROR("datafile '%s' is corrupt", datafile->getName(datafile));
  }

  // change to read-write if no footer has been found
  else {
    if (! datafile->_isSealed) {
      datafile->_state = TRI_DF_STATE_WRITE;
      TRI_ProtectMMFile(datafile->_data, datafile->_maximalSize, PROT_READ | PROT_WRITE, datafile->_fd, &(datafile->_mmHandle));
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

    res = TRI_UNMMFile(datafile->_data, datafile->_maximalSize, datafile->_fd, &(datafile->_mmHandle));

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
    LOG_WARNING("closing a already closed datafile '%s'", datafile->getName(datafile));
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
  int res;

  // this function must not be called for non-physical datafiles
  assert(datafile->isPhysical(datafile));
  assert(filename != NULL);

  if (TRI_ExistsFile(filename)) {
    LOG_ERROR("cannot overwrite datafile '%s'", filename);

    datafile->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS);
    return false;
  }

  res = TRI_RenameFile(datafile->_filename, filename);

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
/// @brief seals a database, writes a footer, sets it to read-only
////////////////////////////////////////////////////////////////////////////////

int TRI_SealDatafile (TRI_datafile_t* datafile) {
  TRI_df_footer_marker_t footer;
  TRI_df_marker_t* position;
  bool ok;
  int res;

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
  TRI_InitMarker(&footer.base, TRI_DF_MARKER_FOOTER, sizeof(TRI_df_footer_marker_t));
  // set a proper tick value
  footer.base._tick = datafile->_tickMax;

  // reserve space and write footer to file
  datafile->_footerSize = 0;

  res = TRI_ReserveElementDatafile(datafile, footer.base._size, &position, 0);

  if (res == TRI_ERROR_NO_ERROR) {
    res = TRI_WriteCrcElementDatafile(datafile, position, &footer.base, footer.base._size, true);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // sync file
  ok = datafile->sync(datafile, datafile->_data, ((char*) datafile->_data) + datafile->_currentSize);

  if (! ok) {
    // TODO: remove disastrous call to abort() here!! FIXME
    abort();
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
  TRI_ProtectMMFile(datafile->_data, datafile->_maximalSize, PROT_READ, datafile->_fd, &(datafile->_mmHandle));

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
      // TODO: remove disastrous call to abort() here!! FIXME
      abort();
      LOG_ERROR("cannot truncate datafile '%s': %s", datafile->getName(datafile), TRI_last_error());
      datafile->_lastError = TRI_set_errno(TRI_ERROR_SYS_ERROR);
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
////////////////////////////////////////////////////////////////////////////////

int TRI_TruncateDatafile (char const* path, TRI_voc_size_t position) {
  TRI_datafile_t* datafile;
  int res;

  // this function must not be called for non-physical datafiles
  assert(path != NULL);

  datafile = OpenDatafile(path, true);

  if (datafile == NULL) {
    return TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
  }

  res = TruncateAndSealDatafile(datafile, position);
  TRI_CloseDatafile(datafile);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the datafile
////////////////////////////////////////////////////////////////////////////////

TRI_df_scan_t TRI_ScanDatafile (char const* path) {
  TRI_df_scan_t scan;
  TRI_datafile_t* datafile;

  // this function must not be called for non-physical datafiles
  assert(path != NULL);

  datafile = OpenDatafile(path, true);

  if (datafile != 0) {
    scan = ScanDatafile(datafile);
    TRI_CloseDatafile(datafile);
  }
  else {
    scan._currentSize = 0;
    scan._maximalSize = 0;
    scan._endPosition = 0;
    scan._numberMarkers = 0;

    TRI_InitVector(&scan._entries, TRI_CORE_MEM_ZONE, sizeof(TRI_df_scan_entry_t));

    scan._status = 5;
  }

  return scan;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys information about the datafile
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDatafileScan (TRI_df_scan_t* scan) {
  TRI_DestroyVector(&scan->_entries);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
