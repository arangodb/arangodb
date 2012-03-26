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

#include <BasicsC/conversions.h>
#include <BasicsC/files.h>
#include <BasicsC/json.h>
#include <BasicsC/logging.h>
#include <BasicsC/strings.h>

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

static void InitCollection (TRI_collection_t* collection,
                            char* directory,
                            TRI_col_info_t* info) {
  assert(collection);

  memset(collection, 0, sizeof(TRI_collection_t));

  collection->_version = info->_version;
  collection->_type = info->_type;

  collection->_state = TRI_COL_STATE_WRITE;
  collection->_lastError = 0;

  collection->_cid = info->_cid;
  TRI_CopyString(collection->_name, info->_name, sizeof(collection->_name));
  collection->_maximalSize = info->_maximalSize;
  collection->_waitForSync = info->_waitForSync;
  collection->_deleted = false;

  collection->_directory = directory;

  TRI_InitVectorPointer(&collection->_datafiles);
  TRI_InitVectorPointer(&collection->_journals);
  TRI_InitVectorPointer(&collection->_compactors);
  TRI_InitVectorString(&collection->_indexFiles);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a collection
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

  TRI_InitVectorPointer(&journals);
  TRI_InitVectorPointer(&compactors);
  TRI_InitVectorPointer(&datafiles);
  TRI_InitVectorPointer(&sealed);
  TRI_InitVectorPointer(&all);

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
          LOG_ERROR("collection header mismatch, expected TRI_COL_MARKER_HEADER, found %lu",
                    (unsigned long) cm->base._type);

          TRI_FreeString(filename);
          stop = true;
          break;
        }

        if (cm->_cid != collection->_cid) {
          LOG_ERROR("collection identifier mismatch, expected %lu, found %lu",
                    (unsigned long) collection->_cid,
                    (unsigned long) cm->_cid);

          TRI_FreeString(filename);
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
            collection->_lastError = TRI_set_errno(TRI_ERROR_AVOCADO_CORRUPTED_DATAFILE);
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

        TRI_FreeString(filename);
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
      TRI_FreeString(dname);
      TRI_FreeString(number);

      ok = TRI_RenameDatafile(datafile, filename);

      if (ok) {
        TRI_PushBackVectorPointer(&datafiles, datafile);
        LOG_DEBUG("renamed sealed journal to '%s'", filename);
      }
      else {
        collection->_lastError = datafile->_lastError;
        stop = true;

        LOG_ERROR("cannot rename sealed log-file to %s, this should not happen: %s", filename, TRI_errno());

        break;
      }

      TRI_FreeString(filename);
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

void TRI_InitParameterCollection (TRI_col_parameter_t* parameter,
                                  char const* name,
                                  TRI_voc_size_t maximalSize) {
  assert(parameter);
  memset(parameter, 0, sizeof(TRI_col_parameter_t));

  parameter->_type = TRI_COL_TYPE_SIMPLE_DOCUMENT;

  parameter->_waitForSync = true;

  parameter->_maximalSize = (maximalSize / PageSize) * PageSize;

  if (parameter->_maximalSize == 0 && maximalSize != 0) {
    parameter->_maximalSize = PageSize;
  }

  TRI_CopyString(parameter->_name, name, sizeof(parameter->_name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_CreateCollection (TRI_collection_t* collection,
                                        char const* path,
                                        TRI_col_info_t* parameter) {
  char* filename;
  char* tmp1;
  char* tmp2;
  int res;

  // generate a collection identifier
  parameter->_cid = TRI_NewTickVocBase();

  // sanity check
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) > parameter->_maximalSize) {
    TRI_set_errno(TRI_ERROR_AVOCADO_DATAFILE_FULL);

    LOG_ERROR("cannot create datafile '%s' in '%s', maximal size '%u' is too small",
              parameter->_name,
              path,
              (unsigned int) parameter->_maximalSize);

    return NULL;
  }

  if (! TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_AVOCADO_WRONG_VOCBASE_PATH);

    LOG_ERROR("cannot create collection '%s', path is not a directory", path);

    return NULL;
  }

  // blob collection use the name
  if (parameter->_type == TRI_COL_TYPE_BLOB) {
    filename = TRI_Concatenate2File(path, parameter->_name);
  }

  // simple collection use the collection identifier
  else if (parameter->_type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    tmp1 = TRI_StringUInt64(parameter->_cid);
    tmp2 = TRI_Concatenate2String("collection-", tmp1);

    filename = TRI_Concatenate2File(path, tmp2);

    TRI_FreeString(tmp2);
    TRI_FreeString(tmp1);
  }

  // uups
  else {
    TRI_set_errno(TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE);

    LOG_ERROR("cannot create collection '%s' in '%s': unknown type '%d'",
              parameter->_name,
              path,
              (unsigned int) parameter->_type);

    return NULL;
  }

  // directory must not exists
  if (TRI_ExistsFile(filename)) {
    TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_DIRECTORY_ALREADY_EXISTS);
    TRI_FreeString(filename);

    LOG_ERROR("cannot create collection '%s' in '%s', name already exists",
              parameter->_name, path);

    return NULL;
  }

  // create directory
  if (! TRI_CreateDirectory(filename)) {
    LOG_ERROR("cannot create collection '%s' in '%s' as '%s': %s",
              parameter->_name,
              path, 
              filename,
              TRI_last_error());

    TRI_FreeString(filename);

    return NULL;
  }

  // save the parameter block
  res = TRI_SaveParameterInfo(filename, parameter);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeString(filename);

    LOG_ERROR("cannot save collection parameter '%s': '%s'", filename, TRI_last_error());

    return NULL;
  }

  // create collection structure
  if (collection == NULL) {
    collection = TRI_Allocate(sizeof(TRI_collection_t));
  }

  InitCollection(collection, filename, parameter);

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
  TRI_DestroyVectorPointer(&collection->_datafiles);
  TRI_DestroyVectorPointer(&collection->_journals);
  TRI_DestroyVectorPointer(&collection->_compactors);
  TRI_DestroyVectorString(&collection->_indexFiles);
  TRI_FreeString(collection->_directory);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollection (TRI_collection_t* collection) {
  assert(collection);
  TRI_DestroyCollection(collection);
  TRI_Free(collection);
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
////////////////////////////////////////////////////////////////////////////////

int TRI_LoadParameterInfo (char const* path,
                           TRI_col_info_t* parameter) {
  TRI_json_t* json;
  char* filename;
  char* error;
  size_t i;
  size_t n;

  memset(parameter, 0, sizeof(TRI_col_info_t));

  // find parameter file
  filename = TRI_Concatenate2File(path, TRI_COL_PARAMETER_FILE);

  if (! TRI_ExistsFile(filename)) {
    TRI_FreeString(filename);
    return TRI_set_errno(TRI_ERROR_AVOCADO_ILLEGAL_PARAMETER_FILE);
  }

  json = TRI_JsonFile(filename, &error);

  if (json == NULL) {
    TRI_FreeString(error);
    TRI_FreeString(filename);

    LOG_ERROR("cannot open '%s', parameter block not readable: %s", filename, error);

    return TRI_set_errno(TRI_ERROR_AVOCADO_ILLEGAL_PARAMETER_FILE);
  }

  TRI_FreeString(filename);

  if (json->_type != TRI_JSON_ARRAY) {
    LOG_ERROR("cannot open '%s', file does not contain a json array", filename);
    return TRI_set_errno(TRI_ERROR_AVOCADO_ILLEGAL_PARAMETER_FILE);
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

  TRI_FreeJson(json);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a parameter info block to file
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveParameterInfo (char const* path,
                           TRI_col_info_t* info) {
  TRI_json_t* json;
  char* filename;
  bool ok;
  
  filename = TRI_Concatenate2File(path, TRI_COL_PARAMETER_FILE);

  // create a json info object
  json = TRI_CreateArrayJson();

  TRI_Insert3ArrayJson(json, "version",     TRI_CreateNumberJson(info->_version));
  TRI_Insert3ArrayJson(json, "type",        TRI_CreateNumberJson(info->_type)); 
  TRI_Insert3ArrayJson(json, "cid",         TRI_CreateNumberJson(info->_cid));
  TRI_Insert3ArrayJson(json, "name",        TRI_CreateStringCopyJson(info->_name));
  TRI_Insert3ArrayJson(json, "maximalSize", TRI_CreateNumberJson(info->_maximalSize));
  TRI_Insert3ArrayJson(json, "waitForSync", TRI_CreateBooleanJson(info->_waitForSync));
  TRI_Insert3ArrayJson(json, "deleted",     TRI_CreateBooleanJson(info->_deleted));

  // save json info to file
  ok = TRI_SaveJson(filename, json);
  TRI_FreeJson(json);

  if (! ok) {
    LOG_ERROR("cannot save info block '%s': '%s'", filename, TRI_last_error());

    TRI_FreeString(filename);
    return TRI_errno();
  }

  TRI_FreeString(filename);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the parameter info block
////////////////////////////////////////////////////////////////////////////////

bool TRI_UpdateParameterInfoCollection (TRI_collection_t* collection) {
  TRI_col_info_t parameter;

  parameter._version = collection->_version;
  parameter._type = collection->_type;
  parameter._cid = collection->_cid;
  TRI_CopyString(parameter._name, collection->_name, sizeof(parameter._name));
  parameter._maximalSize = collection->_maximalSize;
  parameter._waitForSync = collection->_waitForSync;
  parameter._deleted = collection->_deleted;
  parameter._size = sizeof(TRI_col_info_t);

  return TRI_SaveParameterInfo(collection->_directory, &parameter) == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
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
  parameter._size = sizeof(TRI_col_info_t);

  res = TRI_SaveParameterInfo(collection->_directory, &parameter);

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

  datafiles = TRI_CopyVectorPointer(&collection->_datafiles);
  journals = TRI_CopyVectorPointer(&collection->_journals);
  compactors = TRI_CopyVectorPointer(&collection->_compactors);

  // iterate over all datafiles
  n = datafiles->_length;

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* datafile;
    bool result;

    datafile = datafiles->_buffer[i];

    result = TRI_IterateDatafile(datafile, iterator, data, false);

    if (! result) {
      TRI_FreeVectorPointer(datafiles);
      TRI_FreeVectorPointer(journals);
      TRI_FreeVectorPointer(compactors);

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
      TRI_FreeVectorPointer(datafiles);
      TRI_FreeVectorPointer(journals);
      TRI_FreeVectorPointer(compactors);

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
      TRI_FreeVectorPointer(datafiles);
      TRI_FreeVectorPointer(journals);
      TRI_FreeVectorPointer(compactors);

      return false;
    }
  }

  TRI_FreeVectorPointer(datafiles);
  TRI_FreeVectorPointer(journals);
  TRI_FreeVectorPointer(compactors);

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

TRI_collection_t* TRI_OpenCollection (TRI_collection_t* collection,
                                      char const* path) {
  TRI_col_info_t info;
  bool freeCol;
  bool ok;
  int res;

  freeCol = false;

  if (! TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_AVOCADO_WRONG_VOCBASE_PATH);

    LOG_ERROR("cannot open '%s', not a directory or not found", path);

    return NULL;
  }

  // read parameter block
  res = TRI_LoadParameterInfo(path, &info);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot save collection parameter '%s': '%s'", path, TRI_last_error());
    return NULL;
  }

  // create collection
  if (collection == NULL) {
    collection = TRI_Allocate(sizeof(TRI_collection_t));
    freeCol = true;
  }

  InitCollection(collection, TRI_DuplicateString(path), &info);

  // check for journals and datafiles
  ok = CheckCollection(collection);

  if (! ok) {
    LOG_ERROR("cannot open '%s', check failed", collection->_directory);

    TRI_FreeString(collection->_directory);

    if (freeCol) {
      TRI_Free(collection);
    }

    return NULL;
  }

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseCollection (TRI_collection_t* collection) {
  TRI_datafile_t* datafile;
  size_t n;
  size_t i;

  // close compactor files
  n = collection->_compactors._length;

  for (i = 0;  i < n;  ++i) {
    datafile = collection->_compactors._buffer[i];

    TRI_CloseDatafile(datafile);
  }

  // close journal files
  n = collection->_journals._length;

  for (i = 0;  i < n;  ++i) {
    datafile = collection->_journals._buffer[i];

    TRI_CloseDatafile(datafile);
  }

  // close datafiles
  n = collection->_datafiles._length;

  for (i = 0;  i < n;  ++i) {
    datafile = collection->_datafiles._buffer[i];

    TRI_CloseDatafile(datafile);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
