////////////////////////////////////////////////////////////////////////////////
/// @brief collections
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "collection.h"

#include <regex.h>

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/shape-collection.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-XXX\.ending$/, where XXX is
/// a number, and type and ending are arbitrary letters
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetNumericFilenamePart (const char* filename) {
  char* pos1;
  char* pos2;

  pos1 = strrchr(filename, '.');

  if (pos1 == NULL) {
    return 0;
  }
  
  pos2 = strrchr(filename, '-');

  if (pos2 == NULL || pos2 > pos1) {
    return 0;
  }

  return TRI_UInt64String2(pos2 + 1, pos1 - pos2 - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two filenames, based on the numeric part contained in 
/// the filename
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

  const uint64_t numLeft  = (l->_filename != NULL ? GetNumericFilenamePart(l->_filename) : 0);
  const uint64_t numRight = (r->_filename != NULL ? GetNumericFilenamePart(r->_filename) : 0);

  if (numLeft != numRight) {
    return numLeft < numRight ? -1 : 1;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a vector of filenames, using the numeric parts contained
////////////////////////////////////////////////////////////////////////////////

static void SortFilenames (TRI_vector_string_t* files) {
  if (files->_length < 1) {
    return;
  }

  qsort(files->_buffer, files->_length, sizeof(char*), &FilenameComparator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a vector of datafiles, using the numeric parts contained
////////////////////////////////////////////////////////////////////////////////

static void SortDatafiles (TRI_vector_pointer_t* files) {
  if (files->_length < 1) {
    return;
  }

  qsort(files->_buffer, files->_length, sizeof(TRI_datafile_t*), &DatafileComparator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new collection
////////////////////////////////////////////////////////////////////////////////

static void InitCollection (TRI_vocbase_t* vocbase,
                            TRI_collection_t* collection,
                            char* directory,
                            const TRI_col_info_t* const info) {
  assert(collection);

  memset(collection, 0, sizeof(TRI_collection_t));

  TRI_CopyCollectionInfo(&collection->_info, info);
  
  collection->_vocbase = vocbase;

  collection->_state = TRI_COL_STATE_WRITE;
  collection->_lastError = 0;

  collection->_maximumMarkerSize = 0;

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
  TRI_vector_string_t files;
  regex_t re;
  size_t i;
  size_t n;

  // check files within the directory
  files = TRI_FilesDirectory(path);
  n = files._length;

  regcomp(&re, "^(temp|journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)$", REG_EXTENDED);

  TRI_InitVectorString(&structure._journals, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._compactors, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._datafiles, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._indexes, TRI_CORE_MEM_ZONE);

  for (i = 0;  i < n;  ++i) {
    char const* file = files._buffer[i];
    regmatch_t matches[4];

    if (regexec(&re, file, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      // file type: (journal|datafile|index|compactor)
      char const* first = file + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

      // extension
      char const* third = file + matches[3].rm_so;
      size_t thirdLen = matches[3].rm_eo - matches[3].rm_so;

      // .............................................................................
      // file is an index
      // .............................................................................

      if (TRI_EqualString2("index", first, firstLen) && TRI_EqualString2("json", third, thirdLen)) {
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

        // file is a compactor file
        else if (TRI_EqualString2("compactor", first, firstLen)) {
          TRI_PushBackVectorString(&structure._compactors, filename);
        }

        // file is a datafile
        else if (TRI_EqualString2("datafile", first, firstLen)) {
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
  size_t i;
  size_t n;

  stop = false;

  // check files within the directory
  files = TRI_FilesDirectory(collection->_directory);
  n = files._length;

  regcomp(&re, "^(temp|journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)$", REG_EXTENDED);

  TRI_InitVectorPointer(&journals, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&compactors, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&datafiles, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&sealed, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&all, TRI_UNKNOWN_MEM_ZONE);

  for (i = 0;  i < n;  ++i) {
    char const* file = files._buffer[i];
    regmatch_t matches[4];

    if (regexec(&re, file, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      char const* first = file + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

      char const* third = file + matches[3].rm_so;
      size_t thirdLen = matches[3].rm_eo - matches[3].rm_so;

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

        filename = TRI_Concatenate2File(collection->_directory, file);
        datafile = TRI_OpenDatafile(filename);

        if (datafile == NULL) {
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

        // file is a compactor file
        else if (TRI_EqualString2("compactor", first, firstLen)) {
          if (datafile->_isSealed) {
            LOG_WARNING("strange, compactor journal '%s' is already sealed; must be a left over; will use it as datafile", filename);

            TRI_PushBackVectorPointer(&sealed, datafile);
          }
          else {
            TRI_PushBackVectorPointer(&compactors, datafile);
          }
        }

        // file is a datafile
        else if (TRI_EqualString2("datafile", first, firstLen)) {
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

        // temporary file, we can delete it!
        else if (TRI_EqualString2("temp", first, firstLen)) {
          LOG_WARNING("found temporary file '%s', which is probably a left-over. deleting it", filename);
          TRI_UnlinkFile(filename);
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

      datafile = sealed._buffer[i];

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
      datafile = all._buffer[i];

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
  size_t i;
  size_t n;

  assert(vector);

  n = vector->_length;
  for (i = 0; i < n ; ++i) {
    TRI_datafile_t* datafile = (TRI_datafile_t*) vector->_buffer[i];
    
    LOG_TRACE("freeing collection datafile");

    assert(datafile);
    TRI_FreeDatafile(datafile);
  }

  TRI_DestroyVectorPointer(vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all datafiles in a vector
////////////////////////////////////////////////////////////////////////////////

static bool IterateDatafilesVector (const TRI_vector_pointer_t* const files,
                                    bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*, bool),
                                    void* data) {
  size_t i, n;

  n = files->_length;
  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* datafile;
    int result;

    datafile = (TRI_datafile_t*) TRI_AtVectorPointer(files, i);
    result = TRI_IterateDatafile(datafile, iterator, data, false);

    if (! result) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the datafiles passed in the vector
////////////////////////////////////////////////////////////////////////////////

static bool CloseDataFiles (const TRI_vector_pointer_t* const files) {
  TRI_datafile_t* datafile;
  size_t n = files->_length;
  size_t i;
  bool result = true;
  
  for (i = 0;  i < n;  ++i) {
    datafile = files->_buffer[i];

    assert(datafile);

    result &= TRI_CloseDatafile(datafile);
  }

  return result;
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
/// @brief initializes a collection parameter block
/// (options are added to the TRI_col_info_t* and have to be freed by the 
///  TRI_FreeCollectionInfoOptions() function)
////////////////////////////////////////////////////////////////////////////////

void TRI_InitCollectionInfo (TRI_vocbase_t* vocbase,
                             TRI_col_info_t* parameter,
                             char const* name,
                             TRI_col_type_e type,
                             TRI_voc_size_t maximalSize,
                             TRI_json_t* keyOptions) {
  assert(parameter);
  memset(parameter, 0, sizeof(TRI_col_info_t));

  parameter->_version       = TRI_COL_VERSION;
  parameter->_type          = type;
  parameter->_cid           = 0;
  parameter->_rid           = 0;

  parameter->_deleted       = false;  
  parameter->_isVolatile    = false;
  parameter->_isSystem      = false;
  parameter->_maximalSize   = (maximalSize / PageSize) * PageSize;
  if (parameter->_maximalSize == 0 && maximalSize != 0) {
    parameter->_maximalSize = PageSize;
  }
  parameter->_waitForSync   = vocbase->_defaultWaitForSync;
  parameter->_keyOptions    = keyOptions;
  
  TRI_CopyString(parameter->_name, name, sizeof(parameter->_name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy a collection info block
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyCollectionInfo (TRI_col_info_t* dst, const TRI_col_info_t* const src) {
  assert(dst);
  memset(dst, 0, sizeof(TRI_col_info_t));

  dst->_version       = src->_version;
  dst->_type          = src->_type;
  dst->_cid           = src->_cid;
  dst->_rid           = src->_rid;

  dst->_deleted       = src->_deleted;  
  dst->_isSystem      = src->_isSystem;
  dst->_isVolatile    = src->_isVolatile;
  dst->_maximalSize   = src->_maximalSize;
  dst->_waitForSync   = src->_waitForSync;

  if (src->_keyOptions) {
    dst->_keyOptions  = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, src->_keyOptions);
  }
  else {
    dst->_keyOptions  = NULL;
  }  
  
  TRI_CopyString(dst->_name, src->_name, sizeof(dst->_name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a collection info block
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionInfoOptions (TRI_col_info_t* parameter) {
  if (parameter->_keyOptions != NULL) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameter->_keyOptions);
    parameter->_keyOptions = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the full directory name for a collection
///
/// it is the caller's responsibility to check if the returned string is NULL
/// and to free it if not. 
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetDirectoryCollection (char const* path,
                                  const TRI_col_info_t* const parameter) {
  char* filename;

  assert(path);
  assert(parameter);
  
  // shape collections use just the name, e.g. path/SHAPES
  if (parameter->_type == TRI_COL_TYPE_SHAPE) {
    filename = TRI_Concatenate2File(path, parameter->_name);
  }
  // other collections use the collection identifier
  else if (TRI_IS_DOCUMENT_COLLECTION(parameter->_type)) {
    char* tmp1;
    char* tmp2;

    tmp1 = TRI_StringUInt64(parameter->_cid);
    if (tmp1 == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

      return NULL;
    }

    tmp2 = TRI_Concatenate2String("collection-", tmp1);
    if (tmp2 == NULL) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, tmp1);

      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

      return NULL;
    }

    filename = TRI_Concatenate2File(path, tmp2);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp1);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp2);
  }
  // oops, unknown collection type
  else {
    TRI_set_errno(TRI_ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE);
    return NULL;
  }

  if (filename == NULL) {
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
                                        const TRI_col_info_t* const parameter) {
  char* filename;

  // sanity check
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) > parameter->_maximalSize) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_FULL);

    LOG_ERROR("cannot create datafile '%s' in '%s', maximal size '%u' is too small",
              parameter->_name,
              path,
              (unsigned int) parameter->_maximalSize);

    return NULL;
  }

  if (! TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_ARANGO_WRONG_VOCBASE_PATH);

    LOG_ERROR("cannot create collection '%s', path is not a directory", path);

    return NULL;
  }


  filename = TRI_GetDirectoryCollection(path, parameter);
  if (filename == NULL) {
    LOG_ERROR("cannot create collection '%s'", TRI_last_error());
    return NULL;
  }

  // directory must not exist
  if (TRI_ExistsFile(filename)) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS);

    LOG_ERROR("cannot create collection '%s' in '%s', directory already exists",
              parameter->_name, filename);
    
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return NULL;
  }

  // create directory
  if (! TRI_CreateDirectory(filename)) {
    LOG_ERROR("cannot create collection '%s' in '%s' as '%s': %s",
              parameter->_name,
              path, 
              filename,
              TRI_last_error());

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return NULL;
  }

  // create collection structure
  if (collection == NULL) {
    collection = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_collection_t), false);
    if (collection == NULL) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      LOG_ERROR("cannot create collection '%s', out of memory", path);

      return NULL;
    }
  }

  // we are passing filename to this struct, so we must not free it if you use the struct later
  InitCollection(vocbase, collection, filename, parameter);
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
  assert(collection);
  
  TRI_FreeCollectionInfoOptions(&collection->_info);
          
  FreeDatafilesVector(&collection->_datafiles);
  FreeDatafilesVector(&collection->_journals);
  FreeDatafilesVector(&collection->_compactors);

  TRI_DestroyVectorString(&collection->_indexFiles);
  TRI_FreeString(TRI_CORE_MEM_ZONE, collection->_directory);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollection (TRI_collection_t* collection) {
  assert(collection);
  TRI_DestroyCollection(collection);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
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
/// @brief creates a parameter info block from file
///
/// You must hold the @ref TRI_READ_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_LoadCollectionInfo (char const* path, 
                            TRI_col_info_t* parameter,
                            const bool versionWarning) {
  TRI_json_t* json;
  char* filename;
  char* error = NULL;
  size_t i;
  size_t n;

  memset(parameter, 0, sizeof(TRI_col_info_t));

  // find parameter file
  filename = TRI_Concatenate2File(path, TRI_COL_PARAMETER_FILE);
  if (filename == NULL) {
    LOG_ERROR("cannot load parameter info for collection '%s', out of memory", path);

    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (! TRI_ExistsFile(filename)) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

  json = TRI_JsonFile(TRI_UNKNOWN_MEM_ZONE, filename, &error);

  if (json == NULL) {
    if (error != NULL) {
      LOG_ERROR("cannot open '%s', parameter block not readable: %s", filename, error);
      TRI_FreeString(TRI_CORE_MEM_ZONE, error);
    }
    else {
      LOG_ERROR("cannot open '%s', parameter block not readable", filename);
    }
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

  if (json->_type != TRI_JSON_ARRAY) {
    LOG_ERROR("cannot open '%s', file does not contain a json array", filename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }
  
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  // convert json
  n = json->_value._objects._length;

  for (i = 0;  i < n;  i += 2) {
    TRI_json_t* key;
    TRI_json_t* value;

    key = TRI_AtVector(&json->_value._objects, i);
    value = TRI_AtVector(&json->_value._objects, i + 1);

    if (key->_type == TRI_JSON_STRING && value->_type == TRI_JSON_NUMBER) {
      if (TRI_EqualString(key->_value._string.data, "version")) {
        parameter->_version = value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "type")) {
        parameter->_type = value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "cid")) {
        parameter->_cid = value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "maximalSize")) {
        parameter->_maximalSize = value->_value._number;
      }
    }
    else if (key->_type == TRI_JSON_STRING && value->_type == TRI_JSON_STRING) {
      if (TRI_EqualString(key->_value._string.data, "name")) {
        TRI_CopyString(parameter->_name, value->_value._string.data, sizeof(parameter->_name));

        parameter->_isSystem = TRI_IsSystemCollectionName(parameter->_name);
      }
    }
    else if (key->_type == TRI_JSON_STRING && value->_type == TRI_JSON_BOOLEAN) {
      if (TRI_EqualString(key->_value._string.data, "deleted")) {
        parameter->_deleted = value->_value._boolean;
      }
      else if (TRI_EqualString(key->_value._string.data, "isVolatile")) {
        parameter->_isVolatile = value->_value._boolean;
      }
      else if (TRI_EqualString(key->_value._string.data, "waitForSync")) {
        parameter->_waitForSync = value->_value._boolean;
      }
    }
    else if (key->_type == TRI_JSON_STRING && value->_type == TRI_JSON_ARRAY) {
      if (TRI_EqualString(key->_value._string.data, "keyOptions")) {
        parameter->_keyOptions = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value);
      }
    }
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  // warn about wrong version of the collection
  if (versionWarning && 
      parameter->_type != TRI_COL_TYPE_SHAPE && 
      parameter->_version < TRI_COL_VERSION) {
    if (parameter->_name != NULL) {
      // only warn if the collection version is older than expected, and if it's not a shape collection
      LOG_WARNING("collection '%s' has an old version and needs to be upgraded.", parameter->_name);
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

int TRI_SaveCollectionInfo (char const* path, const TRI_col_info_t* const info) {
  TRI_json_t* json;
  char* filename;
  bool ok;
  
  filename = TRI_Concatenate2File(path, TRI_COL_PARAMETER_FILE);

  // create a json info object
  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (json == NULL) {
    // out of memory
    LOG_ERROR("cannot save info block '%s': out of memory", filename);

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "version",      TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_version));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type",         TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_type)); 
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "cid",          TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_cid));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "deleted",      TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, info->_deleted));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "maximalSize",  TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_maximalSize));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "name",         TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, info->_name));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "isVolatile",   TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, info->_isVolatile));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "waitForSync",  TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, info->_waitForSync));
  
  if (info->_keyOptions) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "keyOptions", TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, info->_keyOptions));
  }

  // save json info to file
  ok = TRI_SaveJson(filename, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (! ok) {
    LOG_ERROR("cannot save info block '%s': '%s'", filename, TRI_last_error());

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    return TRI_errno();
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  return TRI_ERROR_NO_ERROR;
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
                                       TRI_col_info_t const* parameter) {

  if (TRI_IS_DOCUMENT_COLLECTION(collection->_info._type)) {
    TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION((TRI_document_collection_t*) collection);
  }

  if (parameter != 0) {
    collection->_info._maximalSize = parameter->_maximalSize;
    collection->_info._waitForSync = parameter->_waitForSync;

    if (collection->_info._maximalSize < collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD) {
      collection->_info._maximalSize = collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD;
    }
  
    // the following collection properties are intentionally not updated as updating
    // them would be very complicated:
    // - _cid
    // - _name
    // - _type
    // - _isSystem
    // - _isVolatile
    // ... probably a few others missing here ...
  }

  if (TRI_IS_DOCUMENT_COLLECTION(collection->_info._type)) {
    TRI_document_collection_t* docCollection = (TRI_document_collection_t*) collection;

    if (docCollection->base._shaper != NULL) {
      TRI_shape_collection_t* shapeCollection = TRI_CollectionVocShaper(((TRI_document_collection_t*) collection)->base._shaper);

      if (shapeCollection != NULL) {
        // adjust wait for sync value of underlying shape collection
        shapeCollection->base._info._waitForSync = (vocbase->_forceSyncShapes || collection->_info._waitForSync);
      }
    }
    TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION((TRI_document_collection_t*) collection);
  }

  return TRI_SaveCollectionInfo(collection->_directory, &collection->_info);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollection (TRI_collection_t* collection, char const* name) {
  TRI_col_info_t new;
  int res;

  TRI_CopyCollectionInfo(&new, &collection->_info);
  TRI_CopyString(new._name, name, sizeof(new._name));

  res = TRI_SaveCollectionInfo(collection->_directory, &new);

  TRI_FreeCollectionInfoOptions(&new);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_CopyString(collection->_info._name, name, sizeof(collection->_info._name));
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateCollection (TRI_collection_t* collection,
                            bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*, bool),
                            void* data) {
  TRI_vector_pointer_t* datafiles;
  TRI_vector_pointer_t* journals;
  TRI_vector_pointer_t* compactors;
  bool result;

  datafiles = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_datafiles);
  if (datafiles == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  journals = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_journals);
  if (journals == NULL) {
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  compactors = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_compactors);
  if (compactors == NULL) {
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, journals);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

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
/// @brief iterates over all index files of a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateIndexCollection (TRI_collection_t* collection,
                                 bool (*iterator)(char const* filename, void*),
                                 void* data) {
  size_t n;
  size_t i;

  // iterate over all index files
  n = collection->_indexFiles._length;

  for (i = 0;  i < n;  ++i) {
    char const* filename;
    bool ok;

    filename = collection->_indexFiles._buffer[i];
    ok = iterator(filename, data);

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
  TRI_col_info_t info;
  bool freeCol;
  bool ok;
  int res;

  freeCol = false;

  if (! TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_ARANGO_WRONG_VOCBASE_PATH);

    LOG_ERROR("cannot open '%s', not a directory or not found", path);

    return NULL;
  }

  // read parameter block, no need to lock as we are opening the collection
  res = TRI_LoadCollectionInfo(path, &info, true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot load collection parameter '%s': '%s'", path, TRI_last_error());
    return NULL;
  }

  // create collection
  if (collection == NULL) {
    collection = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_collection_t), false);

    if (collection == NULL) {
      LOG_ERROR("cannot open '%s', out of memory", path);

      return NULL;
    }

    freeCol = true;
  }

  InitCollection(vocbase, collection, TRI_DuplicateString(path), &info);
  
  TRI_FreeCollectionInfoOptions(&info);

  // check for journals and datafiles
  ok = CheckCollection(collection);

  if (! ok) {
    LOG_ERROR("cannot open '%s', check failed", collection->_directory);

    TRI_FreeString(TRI_CORE_MEM_ZONE, collection->_directory);

    if (freeCol) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
    }

    return NULL;
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
/// @brief iterate over the markers incollection journals journal
///
/// we do this on startup to find the most recent tick values
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateJournalsCollection (const char* const path,
                                    bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*, bool)) {
  TRI_col_file_structure_t structure = ScanCollectionDirectory(path);
  TRI_vector_string_t* vector = &structure._journals;
  size_t n = vector->_length;

  if (n > 0) {
    size_t i;

    for (i = 0; i < n ; ++i) {
      TRI_datafile_t* datafile;
      char* filename;
 
      filename = TRI_AtVectorString(vector, i);
      LOG_DEBUG("iterating over collection journal file '%s'", filename);
      datafile = TRI_OpenDatafile(filename);
      if (datafile != NULL) {
        TRI_IterateDatafile(datafile, iterator, NULL, true);  
        TRI_CloseDatafile(datafile);
        TRI_FreeDatafile(datafile);
      }
    }
  }

  TRI_DestroyFileStructureCollection(&structure);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a collection name is a system collection name
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemCollectionName (char const* name) {
  if (name == NULL) {
    return false;
  }

  return *name == '_';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type name for a collection
////////////////////////////////////////////////////////////////////////////////

char* TRI_TypeNameCollection (const TRI_col_type_e type) {
  switch (type) {
    case TRI_COL_TYPE_DOCUMENT:
      return "document";
    case TRI_COL_TYPE_EDGE:
      return "edge";
    case TRI_COL_TYPE_SHAPE:
      return "shape";
  }

  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
