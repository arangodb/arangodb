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
#include "VocBase/simple-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new collection
////////////////////////////////////////////////////////////////////////////////

static void InitCollection (TRI_vocbase_t* vocbase,
                            TRI_collection_t* collection,
                            char* directory,
                            TRI_col_info_t* info) {
  assert(collection);

  memset(collection, 0, sizeof(TRI_collection_t));

  collection->_version = info->_version;
  collection->_type = info->_type;
  collection->_vocbase = vocbase;

  collection->_state = TRI_COL_STATE_WRITE;
  collection->_lastError = 0;

  collection->_cid = info->_cid;
  TRI_CopyString(collection->_name, info->_name, sizeof(collection->_name));
  collection->_maximalSize = info->_maximalSize;
  collection->_waitForSync = info->_waitForSync;
  collection->_deleted = false;
  collection->_maximumMarkerSize = 0;

  collection->_directory = directory;

  TRI_InitVectorPointer(&collection->_datafiles, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&collection->_journals, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&collection->_compactors, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorString(&collection->_indexFiles, TRI_UNKNOWN_MEM_ZONE);
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

  regcomp(&re, "^(journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)$", REG_ICASE | REG_EXTENDED);

  TRI_InitVectorString(&structure._journals, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._compactors, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._datafiles, TRI_CORE_MEM_ZONE);
  TRI_InitVectorString(&structure._indexes, TRI_CORE_MEM_ZONE);

  for (i = 0;  i < n;  ++i) {
    char const* file = files._buffer[i];
    regmatch_t matches[4];

    if (regexec(&re, file, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      char const* first = file + matches[1].rm_so;
      size_t firstLen = matches[1].rm_eo - matches[1].rm_so;

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

        // ups, what kind of file is that
        else {
          LOG_ERROR("unknown datafile '%s'", file);
          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
        }
      }
      else {
        LOG_ERROR("unknown datafile '%s'", file);
      }
    }
  }

  TRI_DestroyVectorString(&files);

  regfree(&re);

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
  bool ok;
  bool stop;
  regex_t re;
  size_t i;
  size_t n;

  stop = false;

  // check files within the directory
  files = TRI_FilesDirectory(collection->_directory);
  n = files._length;

  regcomp(&re, "^(journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)$", REG_ICASE | REG_EXTENDED);

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
          stop = true;

          LOG_ERROR("cannot open datafile '%s': %s", filename, TRI_last_error());

          break;
        }

        TRI_PushBackVectorPointer(&all, datafile);

        // check the document header
        ptr = datafile->_data;
        ptr += ((sizeof(TRI_df_header_marker_t) + TRI_DF_BLOCK_ALIGN - 1) / TRI_DF_BLOCK_ALIGN) * TRI_DF_BLOCK_ALIGN;;
        cm = (TRI_col_header_marker_t*) ptr;

        if (cm->base._type != TRI_COL_MARKER_HEADER) {
          LOG_ERROR("collection header mismatch in file '%s', expected TRI_COL_MARKER_HEADER, found %lu",
                    filename,
                    (unsigned long) cm->base._type);

          TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
          stop = true;
          break;
        }

        if (cm->_cid != collection->_cid) {
          LOG_ERROR("collection identifier mismatch, expected %lu, found %lu",
                    (unsigned long) collection->_cid,
                    (unsigned long) cm->_cid);

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

      datafile = sealed._buffer[i];

      number = TRI_StringUInt32(datafile->_fid);
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

  // add the datafiles and journals
  collection->_datafiles = datafiles;
  collection->_journals = journals;
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
////////////////////////////////////////////////////////////////////////////////

void TRI_InitParameterCollection (TRI_vocbase_t* vocbase,
                                  TRI_col_parameter_t* parameter,
                                  char const* name,
                                  TRI_voc_size_t maximalSize) {
  assert(parameter);
  memset(parameter, 0, sizeof(TRI_col_parameter_t));

  parameter->_type = TRI_COL_TYPE_SIMPLE_DOCUMENT;

  parameter->_waitForSync = vocbase->_defaultWaitForSync;
  parameter->_maximalSize = (maximalSize / PageSize) * PageSize;

  parameter->_isSystem = false;

  if (parameter->_maximalSize == 0 && maximalSize != 0) {
    parameter->_maximalSize = PageSize;
  }

  TRI_CopyString(parameter->_name, name, sizeof(parameter->_name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_CreateCollection (TRI_vocbase_t* vocbase,
                                        TRI_collection_t* collection,
                                        char const* path,
                                        TRI_col_info_t* parameter) {
  char* filename;
  char* tmp1;
  char* tmp2;
  int res;

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

  // blob collection use the name
  if (parameter->_type == TRI_COL_TYPE_BLOB) {
    filename = TRI_Concatenate2File(path, parameter->_name);
    /* TODO FIXME: memory allocation might fail */
  }

  // simple collection use the collection identifier
  else if (parameter->_type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    tmp1 = TRI_StringUInt64(parameter->_cid);
    tmp2 = TRI_Concatenate2String("collection-", tmp1);

    filename = TRI_Concatenate2File(path, tmp2);

    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp2);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmp1);
  }

  // uups
  else {
    TRI_set_errno(TRI_ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE);

    LOG_ERROR("cannot create collection '%s' in '%s': unknown type '%d'",
              parameter->_name,
              path,
              (unsigned int) parameter->_type);

    return NULL;
  }

  // directory must not exist
  if (TRI_ExistsFile(filename)) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS);

    LOG_ERROR("cannot create collection '%s' in '%s', filename already exists",
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

  // save the parameter block (within create, no need to lock)
  res = TRI_SaveParameterInfoCollection(filename, parameter);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot save collection parameter '%s': '%s'", filename, TRI_last_error());
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return NULL;
  }

  // create collection structure
  if (collection == NULL) {
    collection = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_collection_t), false);
    /* TODO FIXME: memory allocation might fail */
  }

  InitCollection(vocbase, collection, filename, parameter);

  // return collection
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCollection (TRI_collection_t* collection) {
  assert(collection);

  FreeDatafilesVector(&collection->_datafiles);
  FreeDatafilesVector(&collection->_journals);
  FreeDatafilesVector(&collection->_compactors);

  TRI_DestroyVectorString(&collection->_indexFiles);
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, collection->_directory);
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

int TRI_LoadParameterInfoCollection (char const* path, TRI_col_info_t* parameter) {
  TRI_json_t* json;
  char* filename;
  char* error = NULL;
  size_t i;
  size_t n;

  memset(parameter, 0, sizeof(TRI_col_info_t));

  // find parameter file
  filename = TRI_Concatenate2File(path, TRI_COL_PARAMETER_FILE);
  // TODO: memory allocation might fail

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

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (json->_type != TRI_JSON_ARRAY) {
    LOG_ERROR("cannot open '%s', file does not contain a json array", filename);
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

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
    else if (key->_type == TRI_JSON_STRING && value->_type == TRI_JSON_BOOLEAN) {
      if (TRI_EqualString(key->_value._string.data, "waitForSync")) {
        parameter->_waitForSync = value->_value._boolean;
      }
      else if (TRI_EqualString(key->_value._string.data, "deleted")) {
        parameter->_deleted = value->_value._boolean;
      }
    }
    else if (key->_type == TRI_JSON_STRING && value->_type == TRI_JSON_STRING) {
      if (TRI_EqualString(key->_value._string.data, "name")) {
        TRI_CopyString(parameter->_name, value->_value._string.data, sizeof(parameter->_name));
      }
    }
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a parameter info block to file
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveParameterInfoCollection (char const* path, TRI_col_info_t* info) {
  TRI_json_t* json;
  char* filename;
  bool ok;
  
  filename = TRI_Concatenate2File(path, TRI_COL_PARAMETER_FILE);

  // create a json info object
  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "version",     TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_version));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type",        TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_type)); 
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "cid",         TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_cid));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "name",        TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, info->_name));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "maximalSize", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_maximalSize));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "waitForSync", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, info->_waitForSync));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "deleted",     TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, info->_deleted));

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
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateParameterInfoCollection (TRI_collection_t* collection, TRI_col_parameter_t const* parameter) {
  TRI_col_info_t info;

  if (collection->_type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION((TRI_sim_collection_t*) collection);
  }

  info._version = collection->_version;
  info._type = collection->_type;
  info._cid = collection->_cid;
  TRI_CopyString(info._name, collection->_name, sizeof(info._name));
  info._deleted = collection->_deleted;

  if (parameter != 0) {
    info._maximalSize = parameter->_maximalSize;
    info._waitForSync = parameter->_waitForSync;

    collection->_maximalSize = parameter->_maximalSize;
    collection->_waitForSync = parameter->_waitForSync;

    if (collection->_maximalSize < collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD) {
      collection->_maximalSize = collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD;
    }
  }
  else {
    info._maximalSize = collection->_maximalSize;
    info._waitForSync = collection->_waitForSync;
  }

  if (collection->_type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION((TRI_sim_collection_t*) collection);
  }

  return TRI_SaveParameterInfoCollection(collection->_directory, &info);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollection (TRI_collection_t* collection, char const* name) {
  TRI_col_info_t parameter;
  int res;

  parameter._version = collection->_version;
  parameter._type = collection->_type;
  parameter._cid = collection->_cid;
  TRI_CopyString(parameter._name, name, sizeof(parameter._name));
  parameter._maximalSize = collection->_maximalSize;
  parameter._waitForSync = collection->_waitForSync;
  parameter._deleted = collection->_deleted;

  res = TRI_SaveParameterInfoCollection(collection->_directory, &parameter);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_CopyString(collection->_name, name, sizeof(collection->_name));
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
  size_t i;
  size_t n;

  datafiles = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_datafiles);
  journals = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_journals);
  compactors = TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &collection->_compactors);

  // iterate over all datafiles
  n = datafiles->_length;

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* datafile;
    bool result;

    datafile = datafiles->_buffer[i];

    result = TRI_IterateDatafile(datafile, iterator, data, false);

    if (! result) {
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, journals);
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, compactors);

      return false;
    }
  }

  // iterate over all compactors
  n = compactors->_length;

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* datafile;
    bool result;

    datafile = compactors->_buffer[i];

    result = TRI_IterateDatafile(datafile, iterator, data, false);

    if (! result) {
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, journals);
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, compactors);

      return false;
    }
  }

  // iterate over all journals
  n = journals->_length;

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* datafile;
    bool result;

    datafile = journals->_buffer[i];

    result = TRI_IterateDatafile(datafile, iterator, data, false);

    if (! result) {
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, journals);
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, compactors);

      return false;
    }
  }

  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, datafiles);
  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, journals);
  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, compactors);

  return true;
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
                collection->_name);
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
  res = TRI_LoadParameterInfoCollection(path, &info);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot load collection parameter '%s': '%s'", path, TRI_last_error());
    return NULL;
  }

  // create collection
  if (collection == NULL) {
    collection = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_collection_t), false);
    /* TODO FIXME: memory allocation might fail */
    freeCol = true;
  }

  InitCollection(vocbase, collection, TRI_DuplicateString(path), &info);

  // check for journals and datafiles
  ok = CheckCollection(collection);

  if (! ok) {
    LOG_ERROR("cannot open '%s', check failed", collection->_directory);

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, collection->_directory);

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
