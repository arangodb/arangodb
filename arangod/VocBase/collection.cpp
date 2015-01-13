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
#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-shaper.h"

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
    if (TRI_LookupSidBasicShapeShaper(shape->_sid) != nullptr) {
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
  TRI_vector_string_t files;
  regex_t re;
  uint64_t lastId;
  size_t i, n;

  if (regcomp(&re, "^(journal|datafile|compactor)-[0-9][0-9]*\\.db$", REG_EXTENDED) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return (TRI_voc_tick_t) 1;
  }

  files = TRI_FilesDirectory(path);
  n = files._length;
  lastId = 0;

  for (i = 0;  i < n;  ++i) {
    regmatch_t matches[2];
    char const* file = files._buffer[i];

    if (regexec(&re, file, sizeof(matches) / sizeof(matches[1]), matches, 0) == 0) {
      uint64_t id = GetNumericFilenamePart(file);

      if (lastId == 0 || (id > 0 && id < lastId)) {
        lastId = (id - 1);
      }
    }
  }

  TRI_DestroyVectorString(&files);
  regfree(&re);

  return (TRI_voc_tick_t) lastId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new collection
////////////////////////////////////////////////////////////////////////////////

static void InitCollection (TRI_vocbase_t* vocbase,
                            TRI_collection_t* collection,
                            char* directory,
                            const TRI_col_info_t* const info) {
  TRI_ASSERT(collection);

  memset(collection, 0, sizeof(TRI_collection_t));

  TRI_CopyCollectionInfo(&collection->_info, info);

  collection->_vocbase = vocbase;

  collection->_state = TRI_COL_STATE_WRITE;
  collection->_lastError = 0;

  collection->_directory = directory;
  collection->_tickMax = 0;

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
  TRI_vector_string_t files;
  regex_t re;
  size_t i, n;

  TRI_InitVectorString(&structure._journals, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._compactors, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._datafiles, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._indexes, TRI_CORE_MEM_ZONE);

  if (regcomp(&re, "^(temp|compaction|journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)(\\.dead)?$", REG_EXTENDED) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return structure;
  }

  // check files within the directory
  files = TRI_FilesDirectory(path);
  n = files._length;

  for (i = 0;  i < n;  ++i) {
    char const* file = files._buffer[i];
    regmatch_t matches[5];

    if (regexec(&re, file, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      // file type: (journal|datafile|index|compactor)
      char const* first = file + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

      // extension
      char const* third = file + matches[3].rm_so;
      size_t thirdLen = matches[3].rm_eo - matches[3].rm_so;

      // isdead?
      size_t fourthLen = matches[4].rm_eo - matches[4].rm_so;

      // .............................................................................
      // file is dead
      // .............................................................................

      if (fourthLen > 0) {
        char* filename;

        filename = TRI_Concatenate2File(path, file);

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

        filename = TRI_Concatenate2File(path, file);
        TRI_PushBackVectorString(&structure._indexes, filename);
      }

      // .............................................................................
      // file is a journal or datafile
      // .............................................................................

      else if (TRI_EqualString2("db", third, thirdLen)) {
        char* filename;

        filename = TRI_Concatenate2File(path, file);

        // file is a journal
        if (TRI_EqualString2("journal", first, firstLen)) {
          TRI_PushBackVectorString(&structure._journals, filename);
        }

        // file is a datafile
        else if (TRI_EqualString2("datafile", first, firstLen)) {
          TRI_PushBackVectorString(&structure._datafiles, filename);
        }

        // file is a left-over compaction file. rename it back
        else if (TRI_EqualString2("compaction", first, firstLen)) {
          char* relName;
          char* newName;

          relName = TRI_Concatenate2String("datafile-", file + strlen("compaction-"));
          newName = TRI_Concatenate2File(path, relName);
          TRI_FreeString(TRI_CORE_MEM_ZONE, relName);

          if (TRI_ExistsFile(newName)) {
            // we have a compaction-xxxx and a datafile-xxxx file. we'll keep the datafile
            TRI_UnlinkFile(filename);

            LOG_WARNING("removing left-over compaction file '%s'", filename);

            TRI_FreeString(TRI_CORE_MEM_ZONE, newName);
            TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
            continue;
          }
          else {
            int res;

            // this should fail, but shouldn't do any harm either...
            TRI_UnlinkFile(newName);

            // rename the compactor to a datafile
            res = TRI_RenameFile(filename, newName);

            if (res != TRI_ERROR_NO_ERROR) {
              LOG_ERROR("unable to rename compaction file '%s'", filename);

              TRI_FreeString(TRI_CORE_MEM_ZONE, newName);
              TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

              continue;
            }
          }

          TRI_Free(TRI_CORE_MEM_ZONE, filename);

          filename = newName;
          TRI_PushBackVectorString(&structure._datafiles, filename);
        }

        // temporary file, we can delete it!
        else if (TRI_EqualString2("temp", first, firstLen)) {
          LOG_WARNING("found temporary file '%s', which is probably a left-over. deleting it", filename);
          TRI_UnlinkFile(filename);
          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
        }

        // ups, what kind of file is that
        else {
          LOG_ERROR("unknown datafile type '%s'", file);
          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
        }
      }
      else {
        LOG_ERROR("unknown datafile type '%s'", file);
      }
    }
  }

  TRI_DestroyVectorString(&files);

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

static bool CheckCollection (TRI_collection_t* collection) {
  TRI_datafile_t* datafile;
  TRI_vector_pointer_t all;
  TRI_vector_pointer_t compactors;
  TRI_vector_pointer_t datafiles;
  TRI_vector_pointer_t journals;
  TRI_vector_pointer_t sealed;
  TRI_vector_string_t files;
  bool stop;
  regex_t re;
  size_t i, n;

  if (regcomp(&re, "^(temp|compaction|journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)(\\.dead)?$", REG_EXTENDED) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return false;
  }

  stop = false;

  // check files within the directory
  files = TRI_FilesDirectory(collection->_directory);
  n = files._length;

  TRI_InitVectorPointer(&journals, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&compactors, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&datafiles, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&sealed, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&all, TRI_UNKNOWN_MEM_ZONE);

  for (i = 0;  i < n;  ++i) {
    char const* file = files._buffer[i];
    regmatch_t matches[5];

    if (regexec(&re, file, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      char const* first = file + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

      char const* third = file + matches[3].rm_so;
      size_t thirdLen = matches[3].rm_eo - matches[3].rm_so;

      size_t fourthLen = matches[4].rm_eo - matches[4].rm_so;

      // check for temporary & dead files

      if (fourthLen > 0 || TRI_EqualString2("temp", first, firstLen)) {
        // found a temporary file. we can delete it!
        char* filename;

        filename = TRI_Concatenate2File(collection->_directory, file);

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

        filename = TRI_Concatenate2File(collection->_directory, file);
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

          filename = TRI_Concatenate2File(collection->_directory, file);
          relName  = TRI_Concatenate2String("datafile-", file + strlen("compaction-"));
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
          filename = TRI_Concatenate2File(collection->_directory, file);
        }

        TRI_ASSERT(filename != nullptr);
        datafile = TRI_OpenDatafile(filename, true);

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

        if (cm->_cid != collection->_info._cid) {
          LOG_ERROR("collection identifier mismatch, expected %llu, found %llu",
                    (unsigned long long) collection->_info._cid,
                    (unsigned long long) cm->_cid);

          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
          stop = true;
          break;
        }

        // file is a journal
        if (TRI_EqualString2("journal", first, firstLen)) {
          if (datafile->_isSealed) {
            LOG_WARNING("strange, journal '%s' is already sealed; must be a left over; will use it as datafile", filename);

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
          LOG_ERROR("unknown datafile '%s'", file);
        }

        TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
      }
      else {
        LOG_ERROR("unknown datafile '%s'", file);
      }
    }
  }

  TRI_DestroyVectorString(&files);

  regfree(&re);

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
  size_t const n = files->_length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_datafile_t* datafile = static_cast<TRI_datafile_t*>(TRI_AtVectorPointer(files, i));

    LOG_TRACE("iterating over datafile '%s', fid %llu",
              datafile->getName(datafile),
              (unsigned long long) datafile->_fid);

    if (! TRI_IterateDatafile(datafile, iterator, data)) {
      return false;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the collection parameters from the json info passed
////////////////////////////////////////////////////////////////////////////////

static void FillParametersFromJson (TRI_col_info_t* parameter,
                                    TRI_json_t const* json) {
  TRI_ASSERT(parameter != nullptr);
  TRI_ASSERT(TRI_IsObjectJson(json));

  // convert json
  size_t const n = json->_value._objects._length;

  for (size_t i = 0;  i < n;  i += 2) {
    TRI_json_t* key = static_cast<TRI_json_t*>(TRI_AtVector(&json->_value._objects, i));
    TRI_json_t* value = static_cast<TRI_json_t*>(TRI_AtVector(&json->_value._objects, i + 1));

    if (! TRI_IsStringJson(key)) {
      continue;
    }

    if (value->_type == TRI_JSON_NUMBER) {
      if (TRI_EqualString(key->_value._string.data, "version")) {
        parameter->_version = (TRI_col_version_t) value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "type")) {
        parameter->_type = (TRI_col_type_e) (int) value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "cid")) {
        parameter->_cid = (TRI_voc_cid_t) value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "planId")) {
        parameter->_planId = (TRI_voc_cid_t) value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "maximalSize")) {
        parameter->_maximalSize = (TRI_voc_size_t) value->_value._number;
      }
    }
    else if (TRI_IsStringJson(value)) {
      if (TRI_EqualString(key->_value._string.data, "name")) {
        TRI_CopyString(parameter->_name, value->_value._string.data, sizeof(parameter->_name) - 1);

        parameter->_isSystem = TRI_IsSystemNameCollection(parameter->_name);
      }
      else if (TRI_EqualString(key->_value._string.data, "cid")) {
        parameter->_cid = (TRI_voc_cid_t) TRI_UInt64String(value->_value._string.data);
      }
      else if (TRI_EqualString(key->_value._string.data, "planId")) {
        parameter->_planId = (TRI_voc_cid_t) TRI_UInt64String(value->_value._string.data);
      }
    }
    else if (value->_type == TRI_JSON_BOOLEAN) {
      if (TRI_EqualString(key->_value._string.data, "deleted")) {
        parameter->_deleted = value->_value._boolean;
      }
      else if (TRI_EqualString(key->_value._string.data, "doCompact")) {
        parameter->_doCompact = value->_value._boolean;
      }
      else if (TRI_EqualString(key->_value._string.data, "isVolatile")) {
        parameter->_isVolatile = value->_value._boolean;
      }
      else if (TRI_EqualString(key->_value._string.data, "waitForSync")) {
        parameter->_waitForSync = value->_value._boolean;
      }
    }
    else if (value->_type == TRI_JSON_OBJECT) {
      if (TRI_EqualString(key->_value._string.data, "keyOptions")) {
        parameter->_keyOptions = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a collection parameters struct
/// (options are added to the TRI_col_info_t* and have to be freed by the
/// TRI_FreeCollectionInfoOptions() function)
////////////////////////////////////////////////////////////////////////////////

void TRI_InitCollectionInfo (TRI_vocbase_t* vocbase,
                             TRI_col_info_t* parameter,
                             char const* name,
                             TRI_col_type_e type,
                             TRI_voc_size_t maximalSize,
                             TRI_json_t* keyOptions) {
  TRI_ASSERT(parameter);
  memset(parameter, 0, sizeof(TRI_col_info_t));

  parameter->_version       = TRI_COL_VERSION;
  parameter->_type          = type;
  parameter->_cid           = 0;
  parameter->_planId        = 0;
  parameter->_revision      = 0;

  parameter->_deleted       = false;
  parameter->_doCompact     = true;
  parameter->_isVolatile    = false;
  parameter->_isSystem      = false;
  parameter->_maximalSize   = (TRI_voc_size_t) ((maximalSize / PageSize) * PageSize);
  if (parameter->_maximalSize == 0 && maximalSize != 0) {
    parameter->_maximalSize = (TRI_voc_size_t) PageSize;
  }
  parameter->_waitForSync   = vocbase->_settings.defaultWaitForSync;

  parameter->_keyOptions    = nullptr;

  if (keyOptions != nullptr) {
    parameter->_keyOptions  = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
  }

  TRI_CopyString(parameter->_name, name, sizeof(parameter->_name) - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fill a collection info struct from the JSON passed
////////////////////////////////////////////////////////////////////////////////

void TRI_FromJsonCollectionInfo (TRI_col_info_t* dst,
                                 TRI_json_t const* json) {
  FillParametersFromJson(dst, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy a collection info block
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyCollectionInfo (TRI_col_info_t* dst, 
                             TRI_col_info_t const* src) {
  TRI_ASSERT(dst);
  memset(dst, 0, sizeof(TRI_col_info_t));

  dst->_version       = src->_version;
  dst->_type          = src->_type;
  dst->_cid           = src->_cid;
  dst->_planId        = src->_planId;
  dst->_revision      = src->_revision;

  dst->_deleted       = src->_deleted;
  dst->_doCompact     = src->_doCompact;
  dst->_isSystem      = src->_isSystem;
  dst->_isVolatile    = src->_isVolatile;
  dst->_maximalSize   = src->_maximalSize;
  dst->_waitForSync   = src->_waitForSync;

  if (src->_keyOptions) {
    dst->_keyOptions  = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, src->_keyOptions);
  }
  else {
    dst->_keyOptions  = nullptr;
  }

  TRI_CopyString(dst->_name, src->_name, sizeof(dst->_name) - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a collection info block
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionInfoOptions (TRI_col_info_t* parameter) {
  if (parameter->_keyOptions != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameter->_keyOptions);
    parameter->_keyOptions = nullptr;
  }
}

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
                                        TRI_col_info_t const* parameters) {
  // sanity check
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) > parameters->_maximalSize) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_FULL);

    LOG_ERROR("cannot create datafile '%s' in '%s', maximal size '%u' is too small",
              parameters->_name,
              path,
              (unsigned int) parameters->_maximalSize);

    return nullptr;
  }

  if (! TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_INVALID);

    LOG_ERROR("cannot create collection '%s', path is not a directory", path);

    return nullptr;
  }


  char* filename = TRI_GetDirectoryCollection(path,
                                              parameters->_name,
                                              parameters->_type,
                                              parameters->_cid);

  if (filename == nullptr) {
    LOG_ERROR("cannot create collection '%s': %s", parameters->_name, TRI_last_error());
    return nullptr;
  }

  // directory must not exist
  if (TRI_ExistsFile(filename)) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS);

    LOG_ERROR("cannot create collection '%s' in directory '%s': directory already exists",
              parameters->_name, filename);

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return nullptr;
  }

  // create directory
  int res = TRI_CreateDirectory(filename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot create collection '%s' in directory '%s': %s",
              parameters->_name,
              path,
              TRI_errno_string(res));

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return nullptr;
  }

  // create collection structure
  if (collection == nullptr) {
    try {
      collection = new TRI_collection_t();
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

  TRI_FreeCollectionInfoOptions(&collection->_info);

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
  TRI_vector_string_t files;
  regex_t re;
  size_t i, n;
  int res;

  if (regcomp(&re, "^index-[0-9][0-9]*\\.json$", REG_EXTENDED | REG_NOSUB) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  files = TRI_FilesDirectory(collection->_path);
  n = files._length;
  res = TRI_ERROR_NO_ERROR;

  // sort by index id
  SortFilenames(&files);

  for (i = 0;  i < n;  ++i) {
    char const* file = files._buffer[i];

    if (regexec(&re, file, (size_t) 0, nullptr, 0) == 0) {
      char* fqn = TRI_Concatenate2File(collection->_path, file);

      res = filter(collection, fqn, data);
      TRI_FreeString(TRI_CORE_MEM_ZONE, fqn);

      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
  }

  TRI_DestroyVectorString(&files);

  regfree(&re);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief jsonify a parameter info block
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_CreateJsonCollectionInfo (TRI_col_info_t const* info) {
  TRI_json_t* json;
  char* cidString;
  char* planIdString;

  // create a json info object
  json = TRI_CreateObjectJson(TRI_CORE_MEM_ZONE, 9);

  if (json == nullptr) {
    return nullptr;
  }

  cidString = TRI_StringUInt64((uint64_t) info->_cid);

  if (cidString == nullptr) {
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    return nullptr;
  }

  planIdString = TRI_StringUInt64((uint64_t) info->_planId);

  if (planIdString == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, cidString);
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    return nullptr;
  }

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "version",      TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) info->_version));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "type",         TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) info->_type));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "cid",          TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, cidString, strlen(cidString)));

  if (info->_planId > 0) {
    TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "planId",     TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, planIdString, strlen(planIdString)));
  }

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "deleted",      TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, info->_deleted));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "doCompact",    TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, info->_doCompact));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "maximalSize",  TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) info->_maximalSize));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "name",         TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, info->_name, strlen(info->_name)));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "isVolatile",   TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, info->_isVolatile));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "waitForSync",  TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, info->_waitForSync));

  if (info->_keyOptions != nullptr) {
    TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "keyOptions", TRI_CopyJson(TRI_CORE_MEM_ZONE, info->_keyOptions));
  }

  TRI_Free(TRI_CORE_MEM_ZONE, planIdString);
  TRI_Free(TRI_CORE_MEM_ZONE, cidString);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a parameter info block from file
///
/// You must hold the @ref TRI_READ_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_LoadCollectionInfo (char const* path,
                            TRI_col_info_t* parameter,
                            bool versionWarning) {
  TRI_json_t* json;
  char* filename;
  char* error = nullptr;

  memset(parameter, 0, sizeof(TRI_col_info_t));
  parameter->_doCompact  = true;
  parameter->_isVolatile = false;

  // find parameter file
  filename = TRI_Concatenate2File(path, TRI_VOC_PARAMETER_FILE);

  if (filename == nullptr) {
    LOG_ERROR("cannot load parameter info for collection '%s', out of memory", path);

    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (! TRI_ExistsFile(filename)) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

  json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, &error);

  if (! TRI_IsObjectJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }

    if (error != nullptr) {
      LOG_ERROR("cannot open '%s', collection parameters are not readable: %s", filename, error);
      TRI_FreeString(TRI_CORE_MEM_ZONE, error);
    }
    else {
      LOG_ERROR("cannot open '%s', collection parameters are not readable", filename);
    }
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  FillParametersFromJson(parameter, json);

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  // warn about wrong version of the collection
  if (versionWarning && parameter->_version < TRI_COL_VERSION_20) {
    if (parameter->_name[0] != '\0') {
      // only warn if the collection version is older than expected, and if it's not a shape collection
      LOG_WARNING("collection '%s' has an old version and needs to be upgraded.",
                  parameter->_name);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a parameter info block to file
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveCollectionInfo (char const* path,
                            TRI_col_info_t const* info,
                            bool forceSync) {
  TRI_json_t* json;
  char* filename;
  bool ok;

  filename = TRI_Concatenate2File(path, TRI_VOC_PARAMETER_FILE);
  json = TRI_CreateJsonCollectionInfo(info);

  // save json info to file
  ok = TRI_SaveJson(filename, json, forceSync);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the parameter info block
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
/// Note: the parameter pointer might be 0 when a collection gets unloaded!!!!
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateCollectionInfo (TRI_vocbase_t* vocbase,
                              TRI_collection_t* collection,
                              TRI_col_info_t const* parameter,
                              bool doSync) {

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION((TRI_document_collection_t*) collection);

  if (parameter != nullptr) {
    collection->_info._doCompact   = parameter->_doCompact;
    collection->_info._maximalSize = parameter->_maximalSize;
    collection->_info._waitForSync = parameter->_waitForSync;

    // the following collection properties are intentionally not updated as updating
    // them would be very complicated:
    // - _cid
    // - _name
    // - _type
    // - _isSystem
    // - _isVolatile
    // ... probably a few others missing here ...
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION((TRI_document_collection_t*) collection);

  return TRI_SaveCollectionInfo(collection->_directory, &collection->_info, doSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollection (TRI_collection_t* collection,
                          char const* name) {
  TRI_col_info_t newInfo;
  int res;

  TRI_CopyCollectionInfo(&newInfo, &collection->_info);
  TRI_CopyString(newInfo._name, name, sizeof(newInfo._name) - 1);

  res = TRI_SaveCollectionInfo(collection->_directory, &newInfo, true);

  TRI_FreeCollectionInfoOptions(&newInfo);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_CopyString(collection->_info._name, name, sizeof(collection->_info._name) - 1);
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
                collection->_info._name);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_OpenCollection (TRI_vocbase_t* vocbase,
                                      TRI_collection_t* collection,
                                      char const* path) {
  TRI_ASSERT(collection != nullptr);

  if (! TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_INVALID);

    LOG_ERROR("cannot open '%s', not a directory or not found", path);

    return nullptr;
  }

  // read parameters, no need to lock as we are opening the collection
  TRI_col_info_t info;
  int res = TRI_LoadCollectionInfo(path, &info, true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot load collection parameter '%s': %s", path, TRI_last_error());
    return nullptr;
  }

  InitCollection(vocbase, collection, TRI_DuplicateString(path), &info);

  TRI_FreeCollectionInfoOptions(&info);

  // check for journals and datafiles
  bool ok = CheckCollection(collection);

  if (! ok) {
    LOG_DEBUG("cannot open '%s', check failed", collection->_directory);

    if (collection->_directory != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, collection->_directory);
      collection->_directory = nullptr;
    }

    return nullptr;
  }

  return collection;
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
                             TRI_col_info_t* info) {

  regex_t re;
  TRI_vector_string_t files;
  TRI_voc_tick_t datafileId;
  char* shapes;
  char* outfile;
  char* fname;
  char* number;
  ssize_t written;
  size_t i, n;
  int fdout;
  int res;

  TRI_ASSERT(info->_version < TRI_COL_VERSION_20);

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
  files = TRI_FilesDirectory(shapes);
  n = files._length;
  fdout = 0;

  for (i = 0;  i < n;  ++i) {
    regmatch_t matches[1];
    char const* file = files._buffer[i];

    if (regexec(&re, file, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      TRI_datafile_t* df;
      char* fqn = TRI_Concatenate2File(shapes, file);

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

        fdout = TRI_CREATE(outfile, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

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
        cm._type      = (TRI_col_type_t) info->_type;
        cm._cid       = info->_cid;
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

  TRI_DestroyVectorString(&files);
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
    info->_version = TRI_COL_VERSION_20;
    res = TRI_SaveCollectionInfo(path, info, true);
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

  TRI_col_file_structure_t structure = ScanCollectionDirectory(path);
  bool result;

  LOG_TRACE("iterating ticks of journal '%s'", path);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type name for a collection
////////////////////////////////////////////////////////////////////////////////

char const* TRI_TypeNameCollection (const TRI_col_type_e type) {
  switch (type) {
    case TRI_COL_TYPE_DOCUMENT:
      return "document";
    case TRI_COL_TYPE_EDGE:
      return "edge";
    case TRI_COL_TYPE_SHAPE_DEPRECATED:
      return "shape (deprecated)";
    case TRI_COL_TYPE_UNKNOWN:
    default:
      return "unknown";
  }

  TRI_ASSERT(false);
  return "unknown";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
