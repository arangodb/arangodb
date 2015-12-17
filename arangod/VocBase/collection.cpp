////////////////////////////////////////////////////////////////////////////////
/// @brief collections
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

#include "collection.h"

#include <regex.h>

#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/memory-map.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                              COLLECTION MIGRATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief auxilliary struct for shape file iteration
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  int      _fdout;
  ssize_t* _written;
}
shape_iterator_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief callback for shape iteration on upgrade
////////////////////////////////////////////////////////////////////////////////

static bool UpgradeShapeIterator (TRI_df_marker_t const* marker,
                                  void* data,
                                  TRI_datafile_t* datafile) {
  shape_iterator_t* si = static_cast<shape_iterator_t*>(data);
  ssize_t* written     = si->_written;

  // new or updated document
  if (marker->_type == TRI_DF_MARKER_SHAPE) {
    TRI_shape_t const* shape = (TRI_shape_t const*) ((char const*) marker + sizeof(TRI_df_shape_marker_t));

    // if the shape is of basic type, don't copy it
    if (Shaper::lookupSidBasicShape(shape->_sid) != nullptr) {
      return true;
    }

    // copy the shape
    *written = *written + TRI_WRITE(si->_fdout, marker, TRI_DF_ALIGN_BLOCK(marker->_size));
  }
  else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    // copy any attribute found
    *written = *written + TRI_WRITE(si->_fdout, marker, TRI_DF_ALIGN_BLOCK(marker->_size));
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-abc\.ending$/, where abc is
/// a number, and type and ending are arbitrary letters
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetNumericFilenamePart (const char* filename) {
  const char* pos1 = strrchr(filename, '.');

  if (pos1 == nullptr) {
    return 0;
  }

  const char* pos2 = strrchr(filename, '-');

  if (pos2 == nullptr || pos2 > pos1) {
    return 0;
  }

  return TRI_UInt64String2(pos2 + 1, pos1 - pos2 - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort datafile filenames on startup
////////////////////////////////////////////////////////////////////////////////

static int FilenameComparator (const void* lhs, const void* rhs) {
  const char* l = *((char**) lhs);
  const char* r = *((char**) rhs);

  const uint64_t numLeft  = GetNumericFilenamePart(l);
  const uint64_t numRight = GetNumericFilenamePart(r);

  if (numLeft != numRight) {
    return numLeft < numRight ? -1 : 1;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort datafile filenames on startup
////////////////////////////////////////////////////////////////////////////////

static bool FilenameStringComparator (std::string const& lhs, std::string const& rhs) {

  const uint64_t numLeft  = GetNumericFilenamePart(lhs.c_str());
  const uint64_t numRight = GetNumericFilenamePart(rhs.c_str());
  return numLeft < numRight;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief compare two datafiles, based on the numeric part contained in
/// the filename
////////////////////////////////////////////////////////////////////////////////

static int DatafileComparator (const void* lhs, const void* rhs) {
  TRI_datafile_t* l = *((TRI_datafile_t**) lhs);
  TRI_datafile_t* r = *((TRI_datafile_t**) rhs);

  uint64_t const numLeft  = (l->_filename != nullptr ? GetNumericFilenamePart(l->_filename) : 0);
  uint64_t const numRight = (r->_filename != nullptr ? GetNumericFilenamePart(r->_filename) : 0);

  if (numLeft != numRight) {
    return numLeft < numRight ? -1 : 1;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a vector of filenames, using the numeric parts contained
////////////////////////////////////////////////////////////////////////////////

static void SortFilenames (TRI_vector_string_t* files) {
  if (files->_length <= 1) {
    return;
  }

  qsort(files->_buffer, files->_length, sizeof(char*), &FilenameComparator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a vector of datafiles, using the numeric parts contained
////////////////////////////////////////////////////////////////////////////////

static void SortDatafiles (TRI_vector_pointer_t* files) {
  if (files->_length <= 1) {
    return;
  }

  qsort(files->_buffer, files->_length, sizeof(TRI_datafile_t*), &DatafileComparator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a suitable datafile id for a new datafile, based on the
/// ids of existing datafiles/journals/compactors
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_tick_t GetDatafileId (const char* path) {
  regex_t re;
  uint64_t lastId;

  if (regcomp(&re, "^(journal|datafile|compactor)-[0-9][0-9]*\\.db$", REG_EXTENDED) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return (TRI_voc_tick_t) 1;
  }

  std::vector<std::string> files = TRI_FilesDirectory(path);
  lastId = 0;

  for (auto const& file : files) {
    regmatch_t matches[2];

    if (regexec(&re, file.c_str(), sizeof(matches) / sizeof(matches[1]), matches, 0) == 0) {
      uint64_t id = GetNumericFilenamePart(file.c_str());

      if (lastId == 0 || (id > 0 && id < lastId)) {
        lastId = (id - 1);
      }
    }
  }

  regfree(&re);

  return (TRI_voc_tick_t) lastId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a new collection
////////////////////////////////////////////////////////////////////////////////

static void InitCollection (TRI_vocbase_t* vocbase,
                            TRI_collection_t* collection,
                            char* directory,
                            VocbaseCollectionInfo const& info) {
  TRI_ASSERT(collection != nullptr);

  collection->_info.update(info);

  collection->_vocbase   = vocbase;
  collection->_tickMax   = 0;
  collection->_state     = TRI_COL_STATE_WRITE;
  collection->_lastError = 0;
  collection->_directory = directory;

  TRI_InitVectorPointer(&collection->_datafiles, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&collection->_journals, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&collection->_compactors, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorString(&collection->_indexFiles, TRI_CORE_MEM_ZONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans a collection and locates all files
////////////////////////////////////////////////////////////////////////////////

static TRI_col_file_structure_t ScanCollectionDirectory (char const* path) {
  TRI_col_file_structure_t structure;
  regex_t re;

  TRI_InitVectorString(&structure._journals, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._compactors, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._datafiles, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._indexes, TRI_CORE_MEM_ZONE);

  if (regcomp(&re, "^(temp|compaction|journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)(\\.dead)?$", REG_EXTENDED) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return structure;
  }

  // check files within the directory
  std::vector<std::string> files = TRI_FilesDirectory(path);

  for (auto const& file : files) {
    regmatch_t matches[5];

    if (regexec(&re, file.c_str(), sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      // file type: (journal|datafile|index|compactor)
      char const* first = file.c_str() + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

      // extension
      char const* third = file.c_str() + matches[3].rm_so;
      size_t thirdLen = matches[3].rm_eo - matches[3].rm_so;

      // isdead?
      size_t fourthLen = matches[4].rm_eo - matches[4].rm_so;

      // .............................................................................
      // file is dead
      // .............................................................................

      if (fourthLen > 0) {
        char* filename;

        filename = TRI_Concatenate2File(path, file.c_str());

        if (filename != nullptr) {
          LOG_TRACE("removing .dead file '%s'", filename);
          TRI_UnlinkFile(filename);
          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
        }
      }

      // .............................................................................
      // file is an index
      // .............................................................................

      else if (TRI_EqualString2("index", first, firstLen) && TRI_EqualString2("json", third, thirdLen)) {
        char* filename;

        filename = TRI_Concatenate2File(path, file.c_str());
        TRI_PushBackVectorString(&structure._indexes, filename);
      }

      // .............................................................................
      // file is a journal or datafile
      // .............................................................................

      else if (TRI_EqualString2("db", third, thirdLen)) {
        string filename = TRI_Concatenate2File(path, file.c_str());

        // file is a journal
        if (TRI_EqualString2("journal", first, firstLen)) {
          TRI_PushBackVectorString(&structure._journals, TRI_DuplicateString(filename.c_str()));
        }

        // file is a datafile
        else if (TRI_EqualString2("datafile", first, firstLen)) {
          TRI_PushBackVectorString(&structure._datafiles, TRI_DuplicateString(filename.c_str()));
        }

        // file is a left-over compaction file. rename it back
        else if (TRI_EqualString2("compaction", first, firstLen)) {
          char* relName;
          char* newName;

          relName = TRI_Concatenate2String("datafile-", file.c_str() + strlen("compaction-"));
          newName = TRI_Concatenate2File(path, relName);
          TRI_FreeString(TRI_CORE_MEM_ZONE, relName);

          if (TRI_ExistsFile(newName)) {
            // we have a compaction-xxxx and a datafile-xxxx file. we'll keep the datafile
            TRI_UnlinkFile(filename.c_str());

            LOG_WARNING("removing left-over compaction file '%s'", filename.c_str());

            TRI_FreeString(TRI_CORE_MEM_ZONE, newName);
            continue;
          }
          else {
            int res;

            // this should fail, but shouldn't do any harm either...
            TRI_UnlinkFile(newName);

            // rename the compactor to a datafile
            res = TRI_RenameFile(filename.c_str(), newName);

            if (res != TRI_ERROR_NO_ERROR) {
              LOG_ERROR("unable to rename compaction file '%s'", filename.c_str());

              TRI_FreeString(TRI_CORE_MEM_ZONE, newName);

              continue;
            }
          }

          TRI_PushBackVectorString(&structure._datafiles, newName);
        }

        // temporary file, we can delete it!
        else if (TRI_EqualString2("temp", first, firstLen)) {
          LOG_WARNING("found temporary file '%s', which is probably a left-over. deleting it", filename.c_str());
          TRI_UnlinkFile(filename.c_str());
        }

        // ups, what kind of file is that
        else {
          LOG_ERROR("unknown datafile type '%s'", file.c_str());
        }
      }
      else {
        LOG_ERROR("unknown datafile type '%s'", file.c_str());
      }
    }
  }

  regfree(&re);

  // now sort the files in the structures that we created.
  // the sorting allows us to iterate the files in the correct order
  SortFilenames(&structure._journals);
  SortFilenames(&structure._compactors);
  SortFilenames(&structure._datafiles);
  SortFilenames(&structure._indexes);

  return structure;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a collection
///
/// TODO: Use ScanCollectionDirectory
////////////////////////////////////////////////////////////////////////////////

static bool CheckCollection (TRI_collection_t* collection,
                             bool ignoreErrors) {
  TRI_datafile_t* datafile;
  TRI_vector_pointer_t all;
  TRI_vector_pointer_t compactors;
  TRI_vector_pointer_t datafiles;
  TRI_vector_pointer_t journals;
  TRI_vector_pointer_t sealed;
  bool stop;
  regex_t re;

  if (regcomp(&re, "^(temp|compaction|journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)(\\.dead)?$", REG_EXTENDED) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return false;
  }

  stop = false;

  // check files within the directory
  std::vector<std::string> files = TRI_FilesDirectory(collection->_directory);

  TRI_InitVectorPointer(&journals, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&compactors, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&datafiles, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&sealed, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&all, TRI_UNKNOWN_MEM_ZONE);

  for (auto const& file : files) {
    regmatch_t matches[5];

    if (regexec(&re, file.c_str(), sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      char const* first = file.c_str() + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

      char const* third = file.c_str() + matches[3].rm_so;
      size_t thirdLen = matches[3].rm_eo - matches[3].rm_so;

      size_t fourthLen = matches[4].rm_eo - matches[4].rm_so;

      // check for temporary & dead files

      if (fourthLen > 0 || TRI_EqualString2("temp", first, firstLen)) {
        // found a temporary file. we can delete it!
        char* filename;

        filename = TRI_Concatenate2File(collection->_directory, file.c_str());

        LOG_TRACE("found temporary file '%s', which is probably a left-over. deleting it", filename);
        TRI_UnlinkFile(filename);
        TRI_Free(TRI_CORE_MEM_ZONE, filename);
        continue;
      }

      // .............................................................................
      // file is an index, just store the filename
      // .............................................................................

      if (TRI_EqualString2("index", first, firstLen) && TRI_EqualString2("json", third, thirdLen)) {
        char* filename;

        filename = TRI_Concatenate2File(collection->_directory, file.c_str());
        TRI_PushBackVectorString(&collection->_indexFiles, filename);
      }

      // .............................................................................
      // file is a journal or datafile, open the datafile
      // .............................................................................

      else if (TRI_EqualString2("db", third, thirdLen)) {
        char* filename;
        char* ptr;
        TRI_col_header_marker_t* cm;

        if (TRI_EqualString2("compaction", first, firstLen)) {
          // found a compaction file. now rename it back
          char* relName;
          char* newName;

          filename = TRI_Concatenate2File(collection->_directory, file.c_str());
          relName  = TRI_Concatenate2String("datafile-", file.c_str() + strlen("compaction-"));
          newName  = TRI_Concatenate2File(collection->_directory, relName);

          TRI_FreeString(TRI_CORE_MEM_ZONE, relName);

          if (TRI_ExistsFile(newName)) {
            // we have a compaction-xxxx and a datafile-xxxx file. we'll keep the datafile
            LOG_WARNING("removing unfinished compaction file '%s'", filename);
            TRI_UnlinkFile(filename);

            TRI_FreeString(TRI_CORE_MEM_ZONE, newName);
            TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
            continue;
          }
          else {
            int res;

            res = TRI_RenameFile(filename, newName);

            if (res != TRI_ERROR_NO_ERROR) {
              LOG_ERROR("unable to rename compaction file '%s' to '%s'", filename, newName);
              TRI_FreeString(TRI_CORE_MEM_ZONE, newName);
              TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
              stop = true;
              break;
            }
          }

          TRI_Free(TRI_CORE_MEM_ZONE, filename);
          // reuse newName
          filename = newName;
        }
        else {
          filename = TRI_Concatenate2File(collection->_directory, file.c_str());
        }

        TRI_ASSERT(filename != nullptr);
        datafile = TRI_OpenDatafile(filename, ignoreErrors);

        if (datafile == nullptr) {
          collection->_lastError = TRI_errno();
          LOG_ERROR("cannot open datafile '%s': %s", filename, TRI_last_error());

          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
          stop = true;
          break;
        }

        TRI_PushBackVectorPointer(&all, datafile);

        // check the document header
        ptr  = datafile->_data;
        // skip the datafile header
        ptr += TRI_DF_ALIGN_BLOCK(sizeof(TRI_df_header_marker_t));
        cm   = (TRI_col_header_marker_t*) ptr;

        if (cm->base._type != TRI_COL_MARKER_HEADER) {
          LOG_ERROR("collection header mismatch in file '%s', expected TRI_COL_MARKER_HEADER, found %lu",
                    filename,
                    (unsigned long) cm->base._type);

          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
          stop = true;
          break;
        }

        if (cm->_cid != collection->_info.id()) {
          LOG_ERROR("collection identifier mismatch, expected %llu, found %llu",
                    (unsigned long long) collection->_info.id(),
                    (unsigned long long) cm->_cid);

          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
          stop = true;
          break;
        }

        // file is a journal
        if (TRI_EqualString2("journal", first, firstLen)) {
          if (datafile->_isSealed) {
            if (datafile->_state != TRI_DF_STATE_READ) {
              LOG_WARNING("strange, journal '%s' is already sealed; must be a left over; will use it as datafile", filename);
            }

            TRI_PushBackVectorPointer(&sealed, datafile);
          }
          else {
            TRI_PushBackVectorPointer(&journals, datafile);
          }
        }

        // file is a compactor
        else if (TRI_EqualString2("compactor", first, firstLen)) {
          // ignore
        }

        // file is a datafile (or was a compaction file)
        else if (TRI_EqualString2("datafile", first, firstLen) ||
                 TRI_EqualString2("compaction", first, firstLen)) {
          if (! datafile->_isSealed) {
            LOG_ERROR("datafile '%s' is not sealed, this should never happen", filename);

            collection->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);
            TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
            stop = true;
            break;
          }
          else {
            TRI_PushBackVectorPointer(&datafiles, datafile);
          }
        }

        else {
          LOG_ERROR("unknown datafile '%s'", file.c_str());
        }

        TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
      }
      else {
        LOG_ERROR("unknown datafile '%s'", file.c_str());
      }
    }
  }

  regfree(&re);

  size_t i, n;
  // convert the sealed journals into datafiles
  if (! stop) {
    n = sealed._length;

    for (i = 0;  i < n;  ++i) {
      char* number;
      char* dname;
      char* filename;
      bool ok;

      datafile = static_cast<TRI_datafile_t*>(sealed._buffer[i]);

      number = TRI_StringUInt64(datafile->_fid);
      dname = TRI_Concatenate3String("datafile-", number, ".db");
      filename = TRI_Concatenate2File(collection->_directory, dname);

      TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
      TRI_FreeString(TRI_CORE_MEM_ZONE, number);

      ok = TRI_RenameDatafile(datafile, filename);

      if (ok) {
        TRI_PushBackVectorPointer(&datafiles, datafile);
        LOG_DEBUG("renamed sealed journal to '%s'", filename);
      }
      else {
        collection->_lastError = datafile->_lastError;
        stop = true;
        LOG_ERROR("cannot rename sealed log-file to %s, this should not happen: %s", filename, TRI_last_error());
        break;
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    }
  }

  TRI_DestroyVectorPointer(&sealed);

  // stop if necessary
  if (stop) {
    n = all._length;

    for (i = 0;  i < n;  ++i) {
      datafile = static_cast<TRI_datafile_t*>(all._buffer[i]);

      LOG_TRACE("closing datafile '%s'", datafile->_filename);

      TRI_CloseDatafile(datafile);
      TRI_FreeDatafile(datafile);
    }

    TRI_DestroyVectorPointer(&all);
    TRI_DestroyVectorPointer(&datafiles);

    return false;
  }

  TRI_DestroyVectorPointer(&all);

  // sort the datafiles.
  // this allows us to iterate them in the correct order
  SortDatafiles(&datafiles);
  SortDatafiles(&journals);
  SortDatafiles(&compactors);

  // add the datafiles and journals
  collection->_datafiles  = datafiles;
  collection->_journals   = journals;
  collection->_compactors = compactors;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free all datafiles in a vector
////////////////////////////////////////////////////////////////////////////////

static void FreeDatafilesVector (TRI_vector_pointer_t* const vector) {
  TRI_ASSERT(vector != nullptr);

  size_t const n = vector->_length;
  for (size_t i = 0; i < n ; ++i) {
    TRI_datafile_t* datafile = static_cast<TRI_datafile_t*>(vector->_buffer[i]);

    LOG_TRACE("freeing collection datafile");

    TRI_ASSERT(datafile != nullptr);
    TRI_FreeDatafile(datafile);
  }

  TRI_DestroyVectorPointer(vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all datafiles in a vector
////////////////////////////////////////////////////////////////////////////////

static bool IterateDatafilesVector (const TRI_vector_pointer_t* const files,
                                    bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*),
                                    void* data) {
  TRI_ASSERT(iterator != nullptr);

  size_t const n = files->_length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_datafile_t* datafile = static_cast<TRI_datafile_t*>(TRI_AtVectorPointer(files, i));

    LOG_TRACE("iterating over datafile '%s', fid %llu",
              datafile->getName(datafile),
              (unsigned long long) datafile->_fid);

    if (! TRI_IterateDatafile(datafile, iterator, data)) {
      return false;
    }

    if (datafile->isPhysical(datafile) && datafile->_isSealed) {
      TRI_MMFileAdvise(datafile->_data, datafile->_maximalSize,
                       TRI_MADVISE_RANDOM);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the datafiles passed in the vector
////////////////////////////////////////////////////////////////////////////////

static bool CloseDataFiles (const TRI_vector_pointer_t* const files) {
  bool result = true;

  size_t const n = files->_length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_datafile_t* datafile = static_cast<TRI_datafile_t*>(files->_buffer[i]);

    TRI_ASSERT(datafile != nullptr);

    result &= TRI_CloseDatafile(datafile);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over a set of datafiles, identified by filenames
/// note: the files will be opened and closed
////////////////////////////////////////////////////////////////////////////////

static bool IterateFiles (TRI_vector_string_t* vector,
                          bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*),
                          void* data) {
  TRI_ASSERT(iterator != nullptr);

  size_t const n = vector->_length;

  for (size_t i = 0; i < n ; ++i) {

    char* filename = TRI_AtVectorString(vector, i);
    LOG_DEBUG("iterating over collection journal file '%s'", filename);

    TRI_datafile_t* datafile = TRI_OpenDatafile(filename, true);

    if (datafile != nullptr) {
      TRI_IterateDatafile(datafile, iterator, data);
      TRI_CloseDatafile(datafile);
      TRI_FreeDatafile(datafile);
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the full directory name for a collection
///
/// it is the caller's responsibility to check if the returned string is NULL
/// and to free it if not.
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetDirectoryCollection (char const* path,
                                  char const* name,
                                  TRI_col_type_e type,
                                  TRI_voc_cid_t cid) {
  TRI_ASSERT(path != nullptr);
  TRI_ASSERT(name != nullptr);

  char* tmp1 = TRI_StringUInt64(cid);

  if (tmp1 == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  char* tmp2 = TRI_Concatenate2String("collection-", tmp1);

  if (tmp2 == nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp1);

    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  char* filename = TRI_Concatenate2File(path, tmp2);
  TRI_FreeString(TRI_CORE_MEM_ZONE, tmp1);
  TRI_FreeString(TRI_CORE_MEM_ZONE, tmp2);

  if (filename == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  // might be NULL
  return filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_CreateCollection (TRI_vocbase_t* vocbase,
                                        TRI_collection_t* collection,
                                        char const* path,
                                        triagens::arango::VocbaseCollectionInfo const& parameters) {
  // sanity check
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) > parameters.maximalSize()) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_FULL);

    LOG_ERROR("cannot create datafile '%s' in '%s', maximal size '%u' is too small",
              parameters.namec_str(),
              path,
              (unsigned int) parameters.maximalSize());

    return nullptr;
  }

  if (! TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_INVALID);

    LOG_ERROR("cannot create collection '%s', path is not a directory", path);

    return nullptr;
  }


  char* filename = TRI_GetDirectoryCollection(path,
                                              parameters.namec_str(),
                                              parameters.type(),
                                              parameters.id());

  if (filename == nullptr) {
    LOG_ERROR("cannot create collection '%s': %s", parameters.namec_str(), TRI_last_error());
    return nullptr;
  }

  // directory must not exist
  if (TRI_ExistsFile(filename)) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS);

    LOG_ERROR("cannot create collection '%s' in directory '%s': directory already exists",
              parameters.namec_str(), filename);

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return nullptr;
  }

  // use a temporary directory first. this saves us from leaving an empty directory
  // behind, an the server refusing to start
  char* tmpname = TRI_Concatenate2String(filename, ".tmp");

  // create directory
  std::string errorMessage;
  long systemError;
  int res = TRI_CreateDirectory(tmpname, systemError, errorMessage);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot create collection '%s' in directory '%s': %s - %ld - %s",
              parameters.namec_str(),
              path,
              TRI_errno_string(res),
              systemError,
              errorMessage.c_str());

    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return nullptr;
  }

  TRI_IF_FAILURE("CreateCollection::tempDirectory") {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    return nullptr;
  }

  // create a temporary file
  char* tmpfile = TRI_Concatenate2File(tmpname, ".tmp");
  res = TRI_WriteFile(tmpfile, "", 0);
  TRI_FreeString(TRI_CORE_MEM_ZONE, tmpfile);

  TRI_IF_FAILURE("CreateCollection::tempFile") {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    return nullptr;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot create collection '%s' in directory '%s': %s - %ld - %s",
              parameters.namec_str(),
              path,
              TRI_errno_string(res),
              systemError,
              errorMessage.c_str());
    TRI_RemoveDirectory(tmpname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return nullptr;
  }

  TRI_IF_FAILURE("CreateCollection::renameDirectory") {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    return nullptr;
  }

  res = TRI_RenameFile(tmpname, filename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot create collection '%s' in directory '%s': %s - %ld - %s",
              parameters.namec_str(),
              path,
              TRI_errno_string(res),
              systemError,
              errorMessage.c_str());
    TRI_RemoveDirectory(tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return nullptr;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);

  // now we have the collection directory in place with the correct name and a .tmp file in it


  // create collection structure
  if (collection == nullptr) {
    try {
      TRI_collection_t* tmp = new TRI_collection_t(parameters);
      collection = tmp;
     // new TRI_collection_t(parameters);
    }
    catch (std::exception&) {
      collection = nullptr;
    }

    if (collection == nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      LOG_ERROR("cannot create collection '%s': out of memory", path);

      return nullptr;
    }
  }

  // we are passing filename to this struct, so we must not free it if you use the struct later
  InitCollection(vocbase, collection, filename, parameters);
  /* PANAIA: 1) the parameter file if it exists must be removed
             2) if collection
  */

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCollection (TRI_collection_t* collection) {
  TRI_ASSERT(collection);
  collection->_info.clearKeyOptions();

  FreeDatafilesVector(&collection->_datafiles);
  FreeDatafilesVector(&collection->_journals);
  FreeDatafilesVector(&collection->_compactors);

  TRI_DestroyVectorString(&collection->_indexFiles);
  if (collection->_directory != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, collection->_directory);
    collection->_directory = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollection (TRI_collection_t* collection) {
  TRI_ASSERT(collection);
  TRI_DestroyCollection(collection);
  delete collection;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       class VocbaseCollectionInfo
// -----------------------------------------------------------------------------

// Only temporary until merge with Max
VocbaseCollectionInfo::VocbaseCollectionInfo (CollectionInfo const& other)
: _version(TRI_COL_VERSION),
  _type(other.type()),
  _revision(0), // TODO
  _cid(other.id()),
  _planId(0), // TODO
  _maximalSize(other.journalSize()),
  _initialCount(-1),
  _indexBuckets(other.indexBuckets()),
  _isSystem(other.isSystem()),
  _deleted(other.deleted()),
  _doCompact(other.doCompact()),
  _isVolatile(other.isVolatile()),
  _waitForSync(other.waitForSync()) {
  const std::string name = other.name();
  memset(_name, 0, sizeof(_name));
  memcpy(_name, name.c_str(), name.size());

  // TODO!
  // _keyOptions.reset(other.keyOptions()->get());
}


VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             char const* name,
                                             TRI_col_type_e type,
                                             TRI_voc_size_t maximalSize,
                                             VPackSlice const& keyOptions)
: _version(TRI_COL_VERSION),
  _type(type),
  _revision(0),
  _cid(0),
  _planId(0),
  _maximalSize(vocbase->_settings.defaultMaximalSize),
  _initialCount(-1),
  _indexBuckets(TRI_DEFAULT_INDEX_BUCKETS),
  _keyOptions(nullptr),
  _isSystem(false),
  _deleted(false),
  _doCompact(true),
  _isVolatile(false),
  _waitForSync(vocbase->_settings.defaultWaitForSync) {
  _maximalSize = static_cast<TRI_voc_size_t>((maximalSize / PageSize) * PageSize);
  if (_maximalSize == 0 && maximalSize != 0) {
    _maximalSize = static_cast<TRI_voc_size_t>(PageSize);
  }
  memset(_name, 0, sizeof(_name));
  TRI_CopyString(_name, name, sizeof(_name) - 1);

  if (! keyOptions.isNone()) {
    VPackBuilder builder;
    builder.add(keyOptions);
    _keyOptions = builder.steal();
  }
  else {
    // Keep nullptr
  }
}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             char const* name,
                                             VPackSlice const& options) 
: VocbaseCollectionInfo(vocbase, name, TRI_COL_TYPE_DOCUMENT, options) {

}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             char const* name,
                                             TRI_col_type_e type,
                                             VPackSlice const& options)
: _version(TRI_COL_VERSION),
  _type(type),
  _revision(0),
  _cid(0),
  _planId(0),
  _maximalSize(vocbase->_settings.defaultMaximalSize),
  _initialCount(-1),
  _indexBuckets(TRI_DEFAULT_INDEX_BUCKETS),
  _keyOptions(nullptr),
  _isSystem(false),
  _deleted(false),
  _doCompact(true),
  _isVolatile(false),
  _waitForSync(vocbase->_settings.defaultWaitForSync) {

  if (!options.isNone() && options.isObject() ) {
    // TODO what if both are present?
    TRI_voc_size_t maximalSize;
    if (options.hasKey("journalSize")) {
      maximalSize = triagens::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(options, "journalSize", vocbase->_settings.defaultMaximalSize);
    }
    else {
      maximalSize = triagens::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(options, "maximalSize", vocbase->_settings.defaultMaximalSize);
    }

    _maximalSize = static_cast<TRI_voc_size_t>((maximalSize / PageSize) * PageSize);
    if (_maximalSize == 0 && maximalSize != 0) {
      _maximalSize = static_cast<TRI_voc_size_t>(PageSize);
    }

    _doCompact    = triagens::basics::VelocyPackHelper::getBooleanValue(options, "doCompact", true);
    _waitForSync  = triagens::basics::VelocyPackHelper::getBooleanValue(options, "waitForSync", vocbase->_settings.defaultWaitForSync);
    _isVolatile   = triagens::basics::VelocyPackHelper::getBooleanValue(options, "isVolatile", false);
    _isSystem     = (name[0] == '_');
    _indexBuckets = triagens::basics::VelocyPackHelper::getNumericValue<uint32_t>(options, "indexBuckets", TRI_DEFAULT_INDEX_BUCKETS);
    // TODO
    // CHECK data type
    _type = static_cast<TRI_col_type_e>(triagens::basics::VelocyPackHelper::getNumericValue<size_t>(options, "type", _type));
    
    VPackSlice const planIdSlice = options.get("planId");
    TRI_voc_cid_t planId = 0;
    if (planIdSlice.isNumber()) {
      planId = static_cast<TRI_voc_cid_t>(planIdSlice.getNumericValue<uint64_t>());
    }
    else if (planIdSlice.isString()) {
      std::string tmp = planIdSlice.copyString();
      planId = static_cast<TRI_voc_cid_t>(TRI_UInt64String2(tmp.c_str(), tmp.length()));
    }

    if (planId > 0) {
      _planId = planId;
    }
    try {
      if (options.hasKey("keyOptions")) {
        VPackSlice const slice = options.get("keyOptions");
        VPackBuilder builder;
        builder.add(slice);
        _keyOptions = builder.steal();
      }
    }
    catch (...) {
      // Unparseable
      // We keep a nullptr
    }

  }
    
 
  memset(_name, 0, sizeof(_name));
  TRI_CopyString(_name, name, sizeof(_name) - 1);
}

VocbaseCollectionInfo::~VocbaseCollectionInfo () {
  _keyOptions.reset(); // Resets the shared ptr to nullptr
  // If this was the last instance holding it it will be freed
}

VocbaseCollectionInfo VocbaseCollectionInfo::fromFile (char const* path,
                                                       TRI_vocbase_t* vocbase,
                                                       char const* collectionName,
                                                       bool versionWarning) {
  // find parameter file
  char* filename = TRI_Concatenate2File(path, TRI_VOC_PARAMETER_FILE);

  if (filename == nullptr) {
    LOG_ERROR("cannot load parameter info for collection '%s', out of memory", path);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (! TRI_ExistsFile(filename)) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

  std::string filePath (filename, strlen(filename));
  std::shared_ptr<VPackBuilder> content = triagens::basics::VelocyPackHelper::velocyPackFromFile (filePath);
  VPackSlice const slice = content->slice();
  if (! slice.isObject()) {
    LOG_ERROR("cannot open '%s', collection parameters are not readable", filename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  VocbaseCollectionInfo info(vocbase, collectionName, slice);

  // warn about wrong version of the collection
  if (versionWarning && info.version() < TRI_COL_VERSION_20) {
    if (info.name()[0] != '\0') {
      // only warn if the collection version is older than expected, and if it's not a shape collection
      LOG_WARNING("collection '%s' has an old version and needs to be upgraded.",
                  info.namec_str());
    }
  }
  return info;
}

// collection version
TRI_col_version_t VocbaseCollectionInfo::version () const {
  return _version;
}

// collection type
TRI_col_type_e VocbaseCollectionInfo::type () const {
  return _type;
}

// local collection identifier
TRI_voc_cid_t VocbaseCollectionInfo::id () const {
  return _cid;
}

// cluster-wide collection identifier
TRI_voc_cid_t VocbaseCollectionInfo::planId () const {
  return _planId;
}

// last revision id written
TRI_voc_rid_t VocbaseCollectionInfo::revision () const {
  return _revision;
}

// maximal size of memory mapped file
TRI_voc_size_t VocbaseCollectionInfo::maximalSize () const {
  return _maximalSize;
}

// initial count, used when loading a collection
int64_t VocbaseCollectionInfo::initialCount () const {
  return _initialCount;
}

// number of buckets used in hash tables for indexes
uint32_t VocbaseCollectionInfo::indexBuckets () const {
  return _indexBuckets;
}

// name of the collection
std::string VocbaseCollectionInfo::name () const {
  return std::string(_name);
}

// name of the collection as c string
char const* VocbaseCollectionInfo::namec_str () const {
  return _name;
}

// options for key creation
std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> VocbaseCollectionInfo::keyOptions () const {
  return _keyOptions;
}

// If true, collection has been deleted
bool VocbaseCollectionInfo::deleted () const {
  return _deleted;
}

// If true, collection will be compacted
bool VocbaseCollectionInfo::doCompact () const {
  return _doCompact;
}

// If true, collection is a system collection
bool VocbaseCollectionInfo::isSystem () const {
  return _isSystem;
}

// If true, collection is memory-only
bool VocbaseCollectionInfo::isVolatile () const {
  return _isVolatile;
}

// If true waits for mysnc
bool VocbaseCollectionInfo::waitForSync () const {
  return _waitForSync;
}

void VocbaseCollectionInfo::setVersion (TRI_col_version_t version) {
  _version = version;
}

void VocbaseCollectionInfo::rename (char const* name) {
  TRI_CopyString(_name, name, sizeof(_name) - 1);
}

void VocbaseCollectionInfo::setRevision (TRI_voc_rid_t rid,
                  bool force) {
  if (force || rid > _revision) {
    _revision = rid;
  }
}

void VocbaseCollectionInfo::setCollectionId (TRI_voc_cid_t cid) {
  _cid = cid;
}

void VocbaseCollectionInfo::updateCount (size_t size) {
  _initialCount = size;
}

void VocbaseCollectionInfo::setPlanId (TRI_voc_cid_t planId) {
  _planId = planId;
}

void VocbaseCollectionInfo::setDeleted (bool deleted) {
  _deleted = deleted;
}

void VocbaseCollectionInfo::clearKeyOptions () {
  _keyOptions.reset();
}

int VocbaseCollectionInfo::saveToFile (char const* path,
                                       bool forceSync) const {
  char* filename = TRI_Concatenate2File(path, TRI_VOC_PARAMETER_FILE);
  TRI_json_t* json = TRI_CreateJsonCollectionInfo(*this);

  // save json info to file
  bool ok = TRI_SaveJson(filename, json, forceSync);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  int res;
  if (! ok) {
    res = TRI_errno();
    LOG_ERROR("cannot save collection properties file '%s': %s", filename, TRI_last_error());
  }
  else {
    res = TRI_ERROR_NO_ERROR;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  return res;
}

void VocbaseCollectionInfo::update (VPackSlice const& slice,
                                    bool preferDefaults,
                                    TRI_vocbase_t const* vocbase) {

  // the following collection properties are intentionally not updated as updating
  // them would be very complicated:
  // - _cid
  // - _name
  // - _type
  // - _isSystem
  // - _isVolatile
  // ... probably a few others missing here ...

  if (preferDefaults) {
    if (vocbase != nullptr) {
      _doCompact = triagens::basics::VelocyPackHelper::getBooleanValue(slice, "doCompact", true);
      _waitForSync = triagens::basics::VelocyPackHelper::getBooleanValue(slice, "waitForSync", vocbase->_settings.defaultWaitForSync);
      _maximalSize = triagens::basics::VelocyPackHelper::getNumericValue<int>(slice, "maximalSize", vocbase->_settings.defaultMaximalSize);
      // TODO: verify In this case the indexBuckets are not updated
      // _indexBuckets = triagens::velocyPackHelper::getNumericValue<uint32_t>(slice, "indexBuckets", TRI_DEFAULT_INDEX_BUCKETS);
    }
    else {
      _doCompact = triagens::basics::VelocyPackHelper::getBooleanValue(slice, "doCompact", true);
      _waitForSync = triagens::basics::VelocyPackHelper::getBooleanValue(slice, "waitForSync", false);
      _maximalSize = triagens::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(slice, "maximalSize", TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE);
      _indexBuckets = triagens::basics::VelocyPackHelper::getNumericValue<uint32_t>(slice, "indexBuckets", TRI_DEFAULT_INDEX_BUCKETS);
    }
  }
  else {
    _doCompact = triagens::basics::VelocyPackHelper::getBooleanValue(slice, "doCompact", _doCompact);
    _waitForSync = triagens::basics::VelocyPackHelper::getBooleanValue(slice, "waitForSync", _waitForSync);
    _maximalSize = triagens::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(slice, "maximalSize", _maximalSize);
    _indexBuckets = triagens::basics::VelocyPackHelper::getNumericValue<uint32_t>(slice, "indexBuckets", _indexBuckets);
  }
}

void VocbaseCollectionInfo::update (VocbaseCollectionInfo const& other) {
  _version       = other.version();
  _type          = other.type();
  _cid           = other.id();
  _planId        = other.planId();
  _revision      = other.revision();
  _maximalSize   = other.maximalSize();
  _initialCount  = other.initialCount();
  _indexBuckets  = other.indexBuckets();

  TRI_CopyString(_name, other.namec_str(), sizeof(_name) - 1);

  // TODO Ask Max if the old pointer is freed properly
  // _keyOptions = other.keyOptions();

  _deleted       = other.deleted();
  _doCompact     = other.doCompact();
  _isSystem      = other.isSystem();
  _isVolatile    = other.isVolatile();
  _waitForSync   = other.waitForSync();

}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return JSON information about the collection from the collection's
/// "parameter.json" file. This function does not require the collection to be
/// loaded.
/// The caller must make sure that the "parameter.json" file is not modified
/// while this function is called.
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ReadJsonCollectionInfo (TRI_vocbase_col_t* collection) {

  char* filename = TRI_Concatenate2File(collection->_path, TRI_VOC_PARAMETER_FILE);

  // load JSON description of the collection
  TRI_json_t* json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, nullptr);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (json == nullptr) {
    return nullptr;
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over the index (JSON) files of a collection, using a callback
/// function for each.
/// This function does not require the collection to be loaded.
/// The caller must make sure that the files are not modified while this
/// function is called.
////////////////////////////////////////////////////////////////////////////////

int TRI_IterateJsonIndexesCollectionInfo (TRI_vocbase_col_t* collection,
                                          int (*filter)(TRI_vocbase_col_t*, char const*, void*),
                                          void* data) {
  regex_t re;
  int res;

  if (regcomp(&re, "^index-[0-9][0-9]*\\.json$", REG_EXTENDED | REG_NOSUB) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  std::vector<std::string> files = TRI_FilesDirectory(collection->_path);
  res = TRI_ERROR_NO_ERROR;

  // sort by index id
  std::sort(files.begin(), files.end(), FilenameStringComparator);

  for (auto const& file : files) {
    if (regexec(&re, file.c_str(), (size_t) 0, nullptr, 0) == 0) {
      char* fqn = TRI_Concatenate2File(collection->_path, file.c_str());

      res = filter(collection, fqn, data);
      TRI_FreeString(TRI_CORE_MEM_ZONE, fqn);

      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
  }

  regfree(&re);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief jsonify a parameter info block
////////////////////////////////////////////////////////////////////////////////

// Only temporary
TRI_json_t* TRI_CreateJsonCollectionInfo (triagens::arango::VocbaseCollectionInfo const& info) {
  try {
    VPackBuilder builder;
    builder.openObject();
    TRI_CreateVelocyPackCollectionInfo(info, builder);
    builder.close();
    return triagens::basics::VelocyPackHelper::velocyPackToJson(builder.slice());
  }
  catch (...) {
    return nullptr;
  }
}

std::shared_ptr<VPackBuilder> TRI_CreateVelocyPackCollectionInfo (triagens::arango::VocbaseCollectionInfo const& info) {
  // This function might throw
  std::shared_ptr<VPackBuilder> builder(new VPackBuilder());
  builder->openObject();
  TRI_CreateVelocyPackCollectionInfo(info, *builder);
  builder->close();
  return builder;
}

void TRI_CreateVelocyPackCollectionInfo (triagens::arango::VocbaseCollectionInfo const& info,
                                         VPackBuilder& builder) {
  // This function might throw
  char* cidString;
  char* planIdString;

  TRI_ASSERT(! builder.isClosed());

  cidString = TRI_StringUInt64((uint64_t) info.id());

  if (cidString == nullptr) {
    // TODO Proper error message
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  planIdString = TRI_StringUInt64((uint64_t) info.planId());

  if (planIdString == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, cidString);

    // TODO Proper error message
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  builder.add("version", VPackValue(info.version()));
  builder.add("type", VPackValue(info.type()));
  builder.add("cid", VPackValue(cidString));

  if (info.planId() > 0) {
    builder.add("planId", VPackValue(planIdString));
  }

  if (info.initialCount() >= 0) {
    builder.add("count", VPackValue(info.initialCount()));
  }
  builder.add("indexBuckets", VPackValue(info.indexBuckets()));
  builder.add("deleted", VPackValue(info.deleted()));
  builder.add("doCompact", VPackValue(info.doCompact()));
  builder.add("maximalSize", VPackValue(info.maximalSize()));
  builder.add("name", VPackValue(info.name()));
  builder.add("isVolatile", VPackValue(info.isVolatile()));
  builder.add("waitForSync", VPackValue(info.waitForSync()));

  auto opts = info.keyOptions();
  if (opts.get() != nullptr) {
    VPackSlice const slice(opts->data());
    builder.add("keyOptions", slice);
  }
  TRI_Free(TRI_CORE_MEM_ZONE, planIdString);
  TRI_Free(TRI_CORE_MEM_ZONE, cidString);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the parameter info block
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
/// Note: the parameter pointer might be 0 when a collection gets unloaded!!!!
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateCollectionInfo (TRI_vocbase_t* vocbase,
                              TRI_collection_t* collection,
                              VPackSlice const& slice,
                              bool doSync) {
  if (! slice.isNone()) {
    TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION((TRI_document_collection_t*) collection);
    // TODO: Verify that true is correct here!
    collection->_info.update(slice, true, vocbase);
    TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION((TRI_document_collection_t*) collection);
  }
  return collection->_info.saveToFile(collection->_directory, doSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollection (TRI_collection_t* collection,
                          char const* name) {
  // Save name for rollback
  std::string oldName = collection->_info.name();
  collection->_info.rename(name);
  int res = collection->_info.saveToFile(collection->_directory,
                                         true);
  if (res != TRI_ERROR_NO_ERROR) {
    // Rollback
    collection->_info.rename(oldName.c_str());
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateCollection (TRI_collection_t* collection,
                            bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*),
                            void* data) {
  TRI_ASSERT(iterator != nullptr);

  TRI_vector_pointer_t* datafiles = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_datafiles);

  if (datafiles == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  TRI_vector_pointer_t* journals = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_journals);

  if (journals == nullptr) {
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  TRI_vector_pointer_t* compactors = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_compactors);

  if (compactors == nullptr) {
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, journals);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  bool result;
  if (! IterateDatafilesVector(datafiles , iterator, data) ||
      ! IterateDatafilesVector(compactors, iterator, data) ||
      ! IterateDatafilesVector(journals,   iterator, data)) {
    result = false;
  }
  else {
    result = true;
  }

  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, journals);
  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, compactors);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file from the indexFiles vector
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveFileIndexCollection (TRI_collection_t* collection,
                                   TRI_idx_iid_t iid) {
  size_t const n = collection->_indexFiles._length;

  for (size_t i = 0;  i < n;  ++i) {
    char const* filename = collection->_indexFiles._buffer[i];

    if (GetNumericFilenamePart(filename) == iid) {
      // found
      TRI_RemoveVectorString(&collection->_indexFiles, i);
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over all index files of a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateIndexCollection (TRI_collection_t* collection,
                                 bool (*iterator)(char const* filename, void*),
                                 void* data) {
  // iterate over all index files
  size_t const n = collection->_indexFiles._length;

  for (size_t i = 0;  i < n;  ++i) {
    char const* filename = collection->_indexFiles._buffer[i];
    bool ok = iterator(filename, data);

    if (! ok) {
      LOG_ERROR("cannot load index '%s' for collection '%s'",
                filename,
                collection->_info.namec_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_OpenCollection (TRI_vocbase_t* vocbase,
                                      TRI_collection_t* collection,
                                      char const* path,
                                      bool ignoreErrors) {
  TRI_ASSERT(collection != nullptr);

  if (! TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_INVALID);

    LOG_ERROR("cannot open '%s', not a directory or not found", path);

    return nullptr;
  }

  try {
    // read parameters, no need to lock as we are opening the collection
    VocbaseCollectionInfo info = VocbaseCollectionInfo::fromFile (path,
                                                                  vocbase,
                                                                  "", // Name will be set later on
                                                                  true);
    InitCollection(vocbase, collection, TRI_DuplicateString(path), info);

    double start = TRI_microtime();

    LOG_ACTION("open-collection { collection: %s/%s }", 
               vocbase->_name,
               collection->_info.namec_str());

    // check for journals and datafiles
    bool ok = CheckCollection(collection, ignoreErrors);

    if (! ok) {
      LOG_DEBUG("cannot open '%s', check failed", collection->_directory);

      if (collection->_directory != nullptr) {
        TRI_FreeString(TRI_CORE_MEM_ZONE, collection->_directory);
        collection->_directory = nullptr;
      }

      return nullptr;
    }
    
    LOG_TIMER((TRI_microtime() - start),
              "open-collection { collection: %s/%s }", 
              vocbase->_name,
              collection->_info.namec_str());

    return collection;
  }
  catch (...) {
    LOG_ERROR("cannot load collection parameter file '%s': %s", path, TRI_last_error());
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseCollection (TRI_collection_t* collection) {
  // close compactor files
  CloseDataFiles(&collection->_compactors);

  // close journal files
  CloseDataFiles(&collection->_journals);

  // close datafiles
  CloseDataFiles(&collection->_datafiles);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the collection files
////////////////////////////////////////////////////////////////////////////////

TRI_col_file_structure_t TRI_FileStructureCollectionDirectory (char const* path) {
  return ScanCollectionDirectory(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the information
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyFileStructureCollection (TRI_col_file_structure_t* info) {
  TRI_DestroyVectorString(&info->_journals);
  TRI_DestroyVectorString(&info->_compactors);
  TRI_DestroyVectorString(&info->_datafiles);
  TRI_DestroyVectorString(&info->_indexes);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade a collection to ArangoDB 2.0+ format
////////////////////////////////////////////////////////////////////////////////

int TRI_UpgradeCollection20 (TRI_vocbase_t* vocbase,
                             char const* path,
                             VocbaseCollectionInfo& info) {

  regex_t re;
  TRI_voc_tick_t datafileId;
  char* shapes;
  char* outfile;
  char* fname;
  char* number;
  ssize_t written;
  int fdout;
  int res;

  TRI_ASSERT(info.version() < TRI_COL_VERSION_20);

  if (regcomp(&re, "^journal|datafile-[0-9][0-9]*\\.db$", REG_EXTENDED) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  shapes = TRI_Concatenate2File(path, "SHAPES");

  if (shapes == nullptr) {
    regfree(&re);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // determine an artificial datafile id
  datafileId = GetDatafileId(path);
  if (datafileId == 0) {
    datafileId = TRI_NewTickServer();
  }

  number = TRI_StringUInt64(datafileId);
  fname  = TRI_Concatenate3String("datafile-", number, ".db");
  TRI_FreeString(TRI_CORE_MEM_ZONE, number);

  outfile = TRI_Concatenate2File(path, fname);
  TRI_FreeString(TRI_CORE_MEM_ZONE, fname);

  if (outfile == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, shapes);
    regfree(&re);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = TRI_ERROR_NO_ERROR;
  written = 0;

  // find all files in the collection directory
  std::vector<std::string> files = TRI_FilesDirectory(shapes);
  fdout = 0;

  for (auto const& file : files) {
    regmatch_t matches[1];

    if (regexec(&re, file.c_str(), sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      TRI_datafile_t* df;
      char* fqn = TRI_Concatenate2File(shapes, file.c_str());

      if (fqn == nullptr) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        break;
      }

      // create the shape datafile
      if (fdout == 0) {
        TRI_df_header_marker_t header;
        TRI_col_header_marker_t cm;
        TRI_voc_tick_t tick;

        if (TRI_ExistsFile(outfile)) {
          TRI_UnlinkFile(outfile);
        }

        fdout = TRI_CREATE(outfile, O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC, S_IRUSR | S_IWUSR);

        if (fdout < 0) {
          TRI_Free(TRI_CORE_MEM_ZONE, fqn);
          LOG_ERROR("cannot create new datafile '%s'", outfile);
          res = TRI_ERROR_CANNOT_WRITE_FILE;
          break;
        }

        tick = TRI_NewTickServer();

        // datafile header
        TRI_InitMarkerDatafile((char*) &header, TRI_DF_MARKER_HEADER, sizeof(TRI_df_header_marker_t));
        header._version     = TRI_DF_VERSION;
        header._maximalSize = 0; // TODO: seems ok to set this to 0, check if this is ok
        header._fid         = tick;
        header.base._tick   = tick;
        header.base._crc    = TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), (char const*) &header.base, header.base._size));

        written += TRI_WRITE(fdout, &header.base, header.base._size);

        // col header
        TRI_InitMarkerDatafile((char*) &cm, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t));
        cm._type      = (TRI_col_type_t) info.type();
        cm._cid       = info.id();
        cm.base._tick = tick;
        cm.base._crc  = TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), (char const*) &cm.base, cm.base._size));

        written += TRI_WRITE(fdout, &cm.base, cm.base._size);
      }

      // open the datafile, and push it into a vector of datafiles
      df = TRI_OpenDatafile(fqn, true);

      if (df == nullptr) {
        res = TRI_errno();
        TRI_Free(TRI_CORE_MEM_ZONE, fqn);
        break;
      }
      else {
        shape_iterator_t si;
        si._fdout = fdout;
        si._written = &written;

        TRI_IterateDatafile(df, UpgradeShapeIterator, &si);

        TRI_CloseDatafile(df);
        TRI_FreeDatafile(df);
      }

      TRI_Free(TRI_CORE_MEM_ZONE, fqn);
    }
  }

  if (fdout > 0) {
    if (res == TRI_ERROR_NO_ERROR) {
      // datafile footer
      TRI_df_footer_marker_t footer;
      TRI_voc_tick_t tick;
      ssize_t minSize;

      tick = TRI_NewTickServer();
      TRI_InitMarkerDatafile((char*) &footer, TRI_DF_MARKER_FOOTER, sizeof(TRI_df_footer_marker_t));
      footer.base._tick = tick;
      footer.base._crc  = TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), (char const*) &footer.base, footer.base._size));

      written += TRI_WRITE(fdout, &footer.base, footer.base._size);

      TRI_CLOSE(fdout);

      // checkout size of written file
      minSize = sizeof(TRI_df_header_marker_t) +
                sizeof(TRI_col_header_marker_t) +
                sizeof(TRI_df_footer_marker_t);

      if (written <= minSize) {
        // new datafile is empty, we can remove it
        LOG_TRACE("removing empty shape datafile");
        TRI_UnlinkFile(outfile);
      }
    }
    else {
      // some error occurred
      TRI_CLOSE(fdout);
      TRI_UnlinkFile(outfile);
    }
  }

  TRI_Free(TRI_CORE_MEM_ZONE, outfile);

  // try to remove SHAPES directory after upgrade
  if (res == TRI_ERROR_NO_ERROR) {
    res = TRI_RemoveDirectory(shapes);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to remove SHAPES directory '%s': %s",
                shapes,
                TRI_errno_string(res));
    }
  }

  TRI_Free(TRI_CORE_MEM_ZONE, shapes);
  regfree(&re);

  if (res == TRI_ERROR_NO_ERROR) {
    // when no error occurred, we'll bump the version number in the collection parameters file.
    info.setVersion(TRI_COL_VERSION_20);
    res = info.saveToFile(path, true);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over the markers in a collection's datafiles
///
/// this function may be called on server startup for all collections, in order
/// to get the last tick value used
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateTicksCollection (const char* const path,
                                 bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*),
                                 void* data) {
  TRI_ASSERT(iterator != nullptr);

  TRI_col_file_structure_t structure = ScanCollectionDirectory(path);
  LOG_TRACE("iterating ticks of journal '%s'", path);

  bool result;

  if (structure._journals._length == 0) {
    // no journal found for collection. should not happen normally, but if
    // it does, we need to grab the ticks from the datafiles, too
    result = IterateFiles(&structure._datafiles, iterator, data);
  }
  else {
    // compactor files don't need to be iterated... they just contain data copied
    // from other files, so their tick values will never be any higher
    result = IterateFiles(&structure._journals, iterator, data);
  }

  TRI_DestroyFileStructureCollection(&structure);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a collection name is a system collection name
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemNameCollection (char const* name) {
  if (name == nullptr) {
    return false;
  }

  return *name == '_';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a collection name is allowed
///
/// Returns true if the name is allowed and false otherwise
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedNameCollection (bool allowSystem,
                                  char const* name) {
  bool ok;
  char const* ptr;
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is allowed
  for (ptr = name;  *ptr;  ++ptr) {
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
      else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    }
    else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') || ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (! ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
