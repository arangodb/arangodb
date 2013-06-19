////////////////////////////////////////////////////////////////////////////////
/// @brief collections
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

#include "collection.h"

#include <regex.h>

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/shape-collection.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              COLLECTION MIGRATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief old-style master pointer (deprecated)
////////////////////////////////////////////////////////////////////////////////

typedef struct old_doc_mptr_s {
  TRI_voc_rid_t          _rid;      // this is the revision identifier
  TRI_voc_fid_t          _fid;      // this is the datafile identifier
  TRI_voc_tick_t         _validTo;  // this is the deletion time (0 if document is not yet deleted)
  void const*            _data;     // this is the pointer to the beginning of the raw marker
  char*                  _key;      // this is the document identifier (string)
}
old_doc_mptr_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the document id. this is used from the UpgradeOpenIterator
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyHeader (TRI_associative_pointer_t* array, void const* key) {
  return TRI_FnvHashString((char const*) key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the document header. this is used from UpgradeOpenIterator
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementDocument (TRI_associative_pointer_t* array, void const* element) {
  TRI_doc_mptr_t const* e = element;
  return TRI_FnvHashString((char const*) e->_key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a document id and a document
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyDocument (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_doc_mptr_t const* e = element;

  char const * k = key;
  return (strcmp(k, e->_key) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief this iterates over all markers of a collection on upgrade
///
/// It will build a standalone temporary index with all documents, without
/// using any of the existing functionality in document-collection.c or
/// primary-collection.c. The reason is that the iteration over datafiles has
/// changed between 1.2 and 1.3, and this function preserves the logic from
/// 1.2. It is thus used to read 1.2-collections.
/// After the iteration, we have all the (surviving) documents in the 
/// temporary primary index, which will then be used to write out the documents
/// to a new datafile.
////////////////////////////////////////////////////////////////////////////////

static bool UpgradeOpenIterator (TRI_df_marker_t const* marker, 
                                 void* data, 
                                 TRI_datafile_t* datafile, 
                                 bool journal) {
  old_doc_mptr_t const* found;
  TRI_associative_pointer_t* primaryIndex;
  TRI_voc_key_t key = NULL;

  primaryIndex = data;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
      marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    TRI_doc_document_key_marker_t const* d = (TRI_doc_document_key_marker_t const*) marker;

    key = ((char*) d) + d->_offsetKey;

    found = TRI_LookupByKeyAssociativePointer(primaryIndex, key);

    // it is a new entry
    if (found == NULL) {
      old_doc_mptr_t* header;

      header = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_mptr_t), true);
      if (header == NULL) {
        return false;
      }

      header->_rid = marker->_tick;
      header->_fid = datafile->_fid;
      header->_validTo = 0;
      header->_data = marker;
      header->_key = ((char*) d) + d->_offsetKey;

      // insert into primary index
      TRI_InsertKeyAssociativePointer(primaryIndex, header->_key, header, false);
    }

    // it is an update, but only if found has a smaller revision identifier
    else if (found->_rid < d->_rid || 
             (found->_rid == d->_rid && found->_fid <= datafile->_fid)) {
      old_doc_mptr_t* newHeader;
      
      newHeader = CONST_CAST(found);

      // update the header info
      newHeader->_rid = d->_rid;
      newHeader->_fid = datafile->_fid;
      newHeader->_data = marker;
      newHeader->_key = ((char*) d) + d->_offsetKey;
      newHeader->_validTo = 0;
    }
  }
  // deletion
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    TRI_doc_deletion_key_marker_t const* d;

    d = (TRI_doc_deletion_key_marker_t const*) marker;
    key = ((char*) d) + d->_offsetKey;

    found = TRI_LookupByKeyAssociativePointer(primaryIndex, key);

    // it is a new entry, so we missed the create
    if (found == NULL) {
      old_doc_mptr_t* header;

      header = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_mptr_t), true);
      if (header == NULL) {
        return false;
      }

      header->_fid     = datafile->_fid;
      header->_rid     = d->_rid;
      header->_validTo = marker->_tick; 
      header->_data    = marker;
      header->_key     = key;

      // insert into indexes
      TRI_InsertKeyAssociativePointer(primaryIndex, header->_key, header, false);
    }

    // it is a real delete
    else if (found->_validTo == 0) {
      old_doc_mptr_t* newHeader;
      
      newHeader = CONST_CAST(found);

      // mark element as deleted
      newHeader->_validTo = marker->_tick; 
      newHeader->_data    = marker;
      newHeader->_key     = key;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-abc\.ending$/, where abc is
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

  regcomp(&re, "^(temp|compaction|journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)$", REG_EXTENDED);

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
  size_t i;
  size_t n;

  stop = false;

  // check files within the directory
  files = TRI_FilesDirectory(collection->_directory);
  n = files._length;

  regcomp(&re, "^(temp|compaction|journal|datafile|index|compactor)-([0-9][0-9]*)\\.(db|json)$", REG_EXTENDED);

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
        
      // check for temporary files

      if (TRI_EqualString2("temp", first, firstLen)) {
        // found a temporary file. we can delete it!
        char* filename;

        filename = TRI_Concatenate2File(collection->_directory, file);

        LOG_WARNING("found temporary file '%s', which is probably a left-over. deleting it", filename);
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

    LOG_TRACE("iterating over datafile '%s', fid %llu", 
              datafile->getName(datafile), 
              (unsigned long long) datafile->_fid);

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
/// @brief iterate over a set of datafiles, identified by filenames
/// note: the files will be opened and closed 
////////////////////////////////////////////////////////////////////////////////

static bool IterateFiles (TRI_vector_string_t* vector,
                          bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*, bool),
                          void* data,
                          bool journal) {
  size_t i, n;
  
  n = vector->_length;

  for (i = 0; i < n ; ++i) {
    TRI_datafile_t* datafile;
    char* filename;

    filename = TRI_AtVectorString(vector, i);
    LOG_DEBUG("iterating over collection journal file '%s'", filename);
    datafile = TRI_OpenDatafile(filename);

    if (datafile != NULL) {
      TRI_IterateDatafile(datafile, iterator, data, journal);
      TRI_CloseDatafile(datafile);
      TRI_FreeDatafile(datafile);
    }
  }

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
  parameter->_tick          = 0;

  parameter->_deleted       = false;
  parameter->_doCompact     = true;
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
  dst->_tick          = src->_tick;

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
  parameter->_doCompact  = true;
  parameter->_isVolatile = false;

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
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

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

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    if (value->_type == TRI_JSON_NUMBER) {
      if (TRI_EqualString(key->_value._string.data, "version")) {
        parameter->_version = value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "type")) {
        parameter->_type = value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "cid")) {
        parameter->_cid = (TRI_voc_cid_t) value->_value._number;
      }
      else if (TRI_EqualString(key->_value._string.data, "maximalSize")) {
        parameter->_maximalSize = value->_value._number;
      }
    }
    else if (value->_type == TRI_JSON_STRING) {
      if (TRI_EqualString(key->_value._string.data, "name")) {
        TRI_CopyString(parameter->_name, value->_value._string.data, sizeof(parameter->_name));

        parameter->_isSystem = TRI_IsSystemCollectionName(parameter->_name);
      }
      else if (TRI_EqualString(key->_value._string.data, "cid")) {
        parameter->_cid = (TRI_voc_cid_t) TRI_UInt64String(value->_value._string.data);
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
    else if (value->_type == TRI_JSON_ARRAY) {
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

int TRI_SaveCollectionInfo (char const* path, 
                            const TRI_col_info_t* const info, 
                            const bool forceSync) {
  TRI_json_t* json;
  char* filename;
  char* cidString;
  bool ok;

  filename = TRI_Concatenate2File(path, TRI_COL_PARAMETER_FILE);
  cidString = TRI_StringUInt64((uint64_t) info->_cid);

  // create a json info object
  json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "version",      TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, info->_version));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "type",         TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, info->_type));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "cid",          TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, cidString));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "deleted",      TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, info->_deleted));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "doCompact",    TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, info->_doCompact));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "maximalSize",  TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, info->_maximalSize));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "name",         TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, info->_name));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "isVolatile",   TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, info->_isVolatile));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "waitForSync",  TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, info->_waitForSync));

  if (info->_keyOptions) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "keyOptions", TRI_CopyJson(TRI_CORE_MEM_ZONE, info->_keyOptions));
  }
  
  TRI_Free(TRI_CORE_MEM_ZONE, cidString);

  // save json info to file
  ok = TRI_SaveJson(filename, json, forceSync);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

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

  return TRI_SaveCollectionInfo(collection->_directory, &collection->_info, vocbase->_forceSyncProperties);
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

  res = TRI_SaveCollectionInfo(collection->_directory, &new, collection->_vocbase->_forceSyncProperties);

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
    LOG_DEBUG("cannot open '%s', check failed", collection->_directory);

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
/// @brief upgrade a collection
///
/// this is called when starting the database for all collections that have a
/// too low (< TRI_COL_VERSION) "version" attribute. 
/// For now, the upgrade procedure will grab all existing journals, datafiles,
/// and compactors files and read them into memory, into a temporary index.
/// It will use the UpgradeOpenIterator function callback for that.
///
/// All documents that are present in that temporary index will be written 
/// out into a new datafile. That datafiles will only contain the existing
/// documents (at most one per unique key), and the order of the documents in 
/// it does not matter. That file can then be read with the 1.3 OpenIterator,
/// which has a slightly different way of determining which document revision
/// is active.
////////////////////////////////////////////////////////////////////////////////

int TRI_UpgradeCollection (TRI_vocbase_t* vocbase, 
                           const char* const path,
                           TRI_col_info_t* info) {
  TRI_vector_string_t files;
  regex_t re;
  size_t i, n;
  int res;
  
  res = TRI_ERROR_NO_ERROR;
            
  TRI_ASSERT_MAINTAINER(info->_version < TRI_COL_VERSION);

  // find all files in the collection directory
  files = TRI_FilesDirectory(path);
  n = files._length;

  // .............................................................................
  // remove all .new files. they are probably left-overs from previous runs
  // .............................................................................
  
  regcomp(&re, "^.*\\.new$", REG_EXTENDED);

  for (i = 0;  i < n;  ++i) {
    regmatch_t matches[1];
    char const* file = files._buffer[i];

    if (regexec(&re, file, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      char* fqn = TRI_Concatenate2File(path, file);
      int r;
    
      r = TRI_UnlinkFile(fqn);

      if (r != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("could not remove previous temporary file '%s': '%s'", fqn, TRI_errno_string(r));
      }
      
      TRI_FreeString(TRI_CORE_MEM_ZONE, fqn);
    }
  }

  regfree(&re);
 
    
  // .............................................................................
  // migrate journals and datafiles
  // .............................................................................

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_vector_pointer_t datafiles;
    TRI_associative_pointer_t primaryIndex;
    char* outfile;
    TRI_voc_size_t neededSize;
    TRI_voc_size_t actualSize;
    ssize_t written;
    size_t markers;
    int fdout;

    outfile = NULL;

    TRI_InitVectorPointer(&datafiles, TRI_CORE_MEM_ZONE);
    // we're interested in these files...
    regcomp(&re, "^(compactor|journal|datafile)-.*\\.db$", REG_EXTENDED);
  
    for (i = 0;  i < n;  ++i) {
      regmatch_t matches[1];
      char const* file = files._buffer[i];

      if (regexec(&re, file, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
        char* fqn = TRI_Concatenate2File(path, file);

        // open the datafile, and push it into a vector of datafiles
        TRI_datafile_t* datafile = TRI_OpenDatafile(fqn);

        if (datafile != NULL) {
          TRI_PushBackVectorPointer(&datafiles, datafile);
        }
        else {
          LOG_WARNING("could not open datafile '%s'", fqn);
          res = TRI_errno();
        }
        
        TRI_FreeString(TRI_CORE_MEM_ZONE, fqn);
      }
    }

    regfree(&re);

    // all datafiles opened


    if (res == TRI_ERROR_NO_ERROR) {
      // build an in-memory index of documents
      TRI_InitAssociativePointer(&primaryIndex,
          TRI_UNKNOWN_MEM_ZONE,
          HashKeyHeader,
          HashElementDocument,
          IsEqualKeyDocument,
          0);

      // read all markers in the existing datafiles
      for (i = 0; i < datafiles._length; ++i) {
        TRI_datafile_t* df = datafiles._buffer[i];

        TRI_IterateDatafile(df, UpgradeOpenIterator, &primaryIndex, false);
      }


      // calculate the length for the new datafile
      markers = 0;
      neededSize = sizeof(TRI_df_header_marker_t) + 
                   sizeof(TRI_col_header_marker_t) + 
                   sizeof(TRI_df_footer_marker_t);

      // go over all documents in the index and calculate the total length
      for (i = 0; i < primaryIndex._nrAlloc; ++i) {
        old_doc_mptr_t* header = primaryIndex._table[i];

        if (header != NULL && header->_validTo == 0) {
          TRI_df_marker_t const* marker = header->_data;

          if (marker != NULL) {
            neededSize += TRI_DF_ALIGN_BLOCK(marker->_size);
            markers++;
          }
        }
      }

      // round up to nearest page size
      actualSize = ((neededSize + PageSize - 1) / PageSize) * PageSize;
      written = 0;

      // generate the name for the new datafile: datafile-xxx.db.new
      { 
        char* fidString;
        char* jname;

        fidString = TRI_StringUInt64(TRI_NewTickVocBase());
        jname = TRI_Concatenate3String("datafile-", fidString, ".db.new");
        TRI_FreeString(TRI_CORE_MEM_ZONE, fidString);

        outfile = TRI_Concatenate2File(path, jname);
        TRI_FreeString(TRI_CORE_MEM_ZONE, jname);
      }

      LOG_INFO("migrating data for collection '%s' (id: %llu, %llu documents) into new datafile '%s'", 
          info->_name, 
          (unsigned long long) info->_cid,
          (unsigned long long) markers,
          outfile);

      // create the outfile
      fdout = TRI_CREATE(outfile, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

      if (fdout < 0) {
        res = TRI_ERROR_CANNOT_WRITE_FILE;
        LOG_ERROR("cannot create new datafile '%s'", outfile);
      }
      else {
        TRI_df_header_marker_t header;
        TRI_col_header_marker_t cm;
        TRI_df_footer_marker_t footer;

        // datafile header
        TRI_InitMarker(&header.base, TRI_DF_MARKER_HEADER, sizeof(TRI_df_header_marker_t), TRI_NewTickVocBase());
        header._version     = TRI_DF_VERSION;
        header._maximalSize = actualSize;
        header._fid         = TRI_NewTickVocBase();
        header.base._crc    = TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), (char const*) &header.base, header.base._size));

        written += TRI_WRITE(fdout, &header.base, header.base._size);

        // col header
        TRI_InitMarker(&cm.base, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t), TRI_NewTickVocBase());
        cm._type     = (TRI_col_type_t) info->_type;
        cm._cid      = info->_cid;
        cm.base._crc = TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), (char const*) &cm.base, cm.base._size));

        written += TRI_WRITE(fdout, &cm.base, cm.base._size);

        // write all surviving documents into the datafile
        for (i = 0; i < primaryIndex._nrAlloc; ++i) {
          old_doc_mptr_t* mptr = primaryIndex._table[i];

          if (mptr != NULL && mptr->_validTo == 0) {
            TRI_df_marker_t const* marker = mptr->_data;

            if (marker != NULL) {
              TRI_doc_document_key_marker_t* doc;
              void* buffer;
              ssize_t w;
              
              assert(marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
                     marker->_type == TRI_DOC_MARKER_KEY_EDGE);

              // copy the old marker
              buffer = TRI_Allocate(TRI_CORE_MEM_ZONE, TRI_DF_ALIGN_BLOCK(marker->_size), true);
              memcpy(buffer, marker, marker->_size);

              doc = (TRI_doc_document_key_marker_t*) buffer;
              // reset _tid value to 0
              doc->_tid = 0;
              // recalc crc
              doc->base._crc = 0;
              doc->base._crc = TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), (char const*) doc, marker->_size));

              w = TRI_WRITE(fdout, buffer, TRI_DF_ALIGN_BLOCK(marker->_size));

              TRI_Free(TRI_CORE_MEM_ZONE, buffer);

              if (w <= 0) {
                res = TRI_ERROR_INTERNAL;
                LOG_ERROR("an error occurred while writing documents into datafile '%s'", outfile);
                break;
              }

              written += w;
            }
          }
        }

        // datafile footer
        TRI_InitMarker(&footer.base, TRI_DF_MARKER_FOOTER, sizeof(TRI_df_footer_marker_t), TRI_NewTickVocBase());
        footer.base._crc = TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), (char const*) &footer.base, footer.base._size));
        written += TRI_WRITE(fdout, &footer.base, footer.base._size);

        TRI_CLOSE(fdout);

      }

      TRI_DestroyAssociativePointer(&primaryIndex);

      // rename target file (by removing the .new suffix)
      if (res == TRI_ERROR_NO_ERROR) {
        char* dst = TRI_DuplicateString2(outfile, strlen(outfile) - strlen(".new"));

        res = TRI_RenameFile(outfile, dst);
        TRI_FreeString(TRI_CORE_MEM_ZONE, dst);
      }

    }

    if (outfile != NULL) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, outfile);
    }

    // close "old" datafiles
    for (i = 0; i < datafiles._length; ++i) {
      TRI_datafile_t* df = datafiles._buffer[i];
      char* old;

      old = TRI_DuplicateString(df->getName(df));

      TRI_CloseDatafile(df);
      TRI_FreeDatafile(df);

      // if no error happened, we'll also rename the old datafiles to ".old"
      if (res == TRI_ERROR_NO_ERROR) {
        char* dst;

        dst = TRI_Concatenate2String(old, ".old");
        res = TRI_RenameFile(old, dst);
        TRI_FreeString(TRI_CORE_MEM_ZONE, dst);
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, old);
    }

    TRI_DestroyVectorPointer(&datafiles);
  }

  TRI_DestroyVectorString(&files);

  if (res == TRI_ERROR_NO_ERROR) {
    // when no error occurred, we'll bump the version number in the collection parameters file.
    info->_version = TRI_COL_VERSION;
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
                                 bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*, bool),
                                 void* data) {

  TRI_col_file_structure_t structure = ScanCollectionDirectory(path);
  bool result;

  LOG_TRACE("iterating ticks of journal '%s'", path);

  if (structure._journals._length == 0) {
    // no journal found for collection. should not happen normally, but if
    // it does, we need to grab the ticks from the datafiles, too
    result = IterateFiles(&structure._datafiles, iterator, data, false);
  }
  else {
    // compactor files don't need to be iterated... they just contain data copied
    // from other files, so their tick values will never be any higher
    result = IterateFiles(&structure._journals, iterator, data, true);
  }
  
  TRI_DestroyFileStructureCollection(&structure);

  return result;
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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
