////////////////////////////////////////////////////////////////////////////////
/// @brief collection key generators
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "key-generator.h"

#include <regex.h>

#include "BasicsC/conversions.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "BasicsC/voc-errors.h"

#include "VocBase/primary-collection.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                        SPECIALIZED KEY GENERATORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                            REVISION KEY GENERATOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief revision keygen private data
////////////////////////////////////////////////////////////////////////////////

typedef struct revision_keygen_s {
  char*   _prefix;        // key prefix
  size_t  _prefixLength;  // length of key prefix
  size_t  _padLength;     // length of 0 bytes used for left padding
  regex_t _regex;         // regex of allowed keys
  bool    _allowUserKeys; // allow keys supplied by user?
}
revision_keygen_t;

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
/// @brief initialise the revision key generator
////////////////////////////////////////////////////////////////////////////////

static int RevisionInit (TRI_key_generator_t* const generator,
                         const TRI_json_t* const options) {
  revision_keygen_t* data;

  data = (revision_keygen_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(revision_keygen_t), false);
  if (data == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // defaults
  data->_prefix        = NULL;
  data->_prefixLength  = 0;
  data->_padLength     = 0;
  data->_allowUserKeys = true;

  // compile regex for key validation
  if (regcomp(&data->_regex, "^(" TRI_VOC_KEY_REGEX ")$", REG_EXTENDED | REG_NOSUB) != 0) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);
    LOG_ERROR("cannot compile regular expression");
    return TRI_ERROR_INTERNAL;
  }

  if (options != NULL) {
    TRI_json_t* option;

    option = TRI_LookupArrayJson(options, "prefix");
    if (option != NULL && option->_type == TRI_JSON_STRING) {
      data->_prefix = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, option->_value._string.data);
      if (data->_prefix == NULL) {
        // OOM
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);

        return TRI_ERROR_NO_ERROR;
      }

      data->_prefixLength = strlen(data->_prefix);
    }

    option = TRI_LookupArrayJson(options, "pad");
    if (option != NULL && option->_type == TRI_JSON_NUMBER) {
      data->_padLength = (size_t) option->_value._number;
    }

    option = TRI_LookupArrayJson(options, "allowUserKeys");
    if (option != NULL && option->_type == TRI_JSON_BOOLEAN) {
      data->_allowUserKeys = option->_value._boolean;
    }
  }

  generator->_data = (void*) data;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the revision key generator
////////////////////////////////////////////////////////////////////////////////

static void RevisionFree (TRI_key_generator_t* const generator) {
  revision_keygen_t* data;

  data = (revision_keygen_t*) generator->_data;
  if (data != NULL) {
    // free regex
    regfree(&data->_regex);

    // free prefix string
    if (data->_prefix != NULL) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, data->_prefix);
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new key
/// the caller must make sure that the outBuffer is big enough to hold at least
/// maxLength + 1 bytes
////////////////////////////////////////////////////////////////////////////////

static int RevisionKey (TRI_key_generator_t* const generator,
                        const size_t maxLength,
                        const TRI_doc_document_key_marker_t* const marker,
                        const char* const userKey,
                        char* const outBuffer,
                        size_t* const outLength) {
  revision_keygen_t* data;
  char* current;

  data = (revision_keygen_t*) generator->_data;
  assert(data != NULL);

  current = outBuffer;

  if (userKey != NULL) {
    size_t userKeyLength;

    // user has specified a key
    if (! data->_allowUserKeys) {
      // we do not allow user-generated keys
      return TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED;
    }

    userKeyLength = strlen(userKey);
    if (userKeyLength > maxLength) {
      // user key is too long
      return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
    }

    // validate user-supplied key
    if (regexec(&data->_regex, userKey, 0, NULL, 0)  != 0) {
      return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
    }

    memcpy(outBuffer, userKey, userKeyLength);
    current += userKeyLength;
  }
  else {
    // user has not specified a key, generate one
    TRI_voc_rid_t revision = marker->_rid;

    if (data->_prefix != NULL && data->_prefixLength > 0) {
      // copy the prefix
      memcpy(current, data->_prefix, data->_prefixLength);
      current += data->_prefixLength;
    }

    if (data->_padLength == 0) {
      current += TRI_StringUInt64InPlace(revision, current);
    }
    else {
      char numBuffer[22]; // a uint64 cannot be longer than this
      size_t length;

      length = TRI_StringUInt64InPlace(revision, (char*) &numBuffer);

      if (length < data->_padLength) {
        // pad with 0s
        size_t padLength;

        padLength = data->_padLength - length;
        memset(current, '0', padLength);
        current += padLength;
      }

      memcpy(current, (char*) &numBuffer, length);
      current += length;
    }
  }

  // add 0 byte
  *current = '\0';

  if (current - outBuffer > (int) maxLength) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  *outLength = (current - outBuffer);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     KEY GENERATOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                          private static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief name of traditional key generator
////////////////////////////////////////////////////////////////////////////////

static const char* Traditional = "traditional";

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
/// @brief check whether the generaror type name is valid
////////////////////////////////////////////////////////////////////////////////

static bool ValidType (const char* const name) {
  if (TRI_EqualString(name, Traditional)) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the generator type from JSON
////////////////////////////////////////////////////////////////////////////////

static const char* GeneratorType (const TRI_json_t* const parameters) {
  TRI_json_t* type;

  if (parameters == NULL || parameters->_type != TRI_JSON_ARRAY) {
    return Traditional;
  }

  type = TRI_LookupArrayJson(parameters, "type");
  if (type == NULL || parameters->_type != TRI_JSON_STRING) {
    return Traditional;
  }

  return parameters->_value._string.data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new generator
////////////////////////////////////////////////////////////////////////////////

static TRI_key_generator_t* CreateGenerator (const TRI_json_t* const parameters,
                                             TRI_primary_collection_t* const collection) {
  TRI_key_generator_t* generator;
  const char* type;

  type = GeneratorType(parameters);
  if (! ValidType(type)) {
    return NULL;
  }

  generator = (TRI_key_generator_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_key_generator_t), false);
  if (generator == NULL) {
    return NULL;
  }

  generator->_data         = NULL;
  generator->_collection   = collection;

  if (TRI_EqualString(type, Traditional)) {
    generator->init        = &RevisionInit;
    generator->generate    = &RevisionKey;
    generator->free        = &RevisionFree;
  }

  return generator;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a key generator and attach it to the collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateKeyGenerator (const TRI_json_t* const parameters,
                            TRI_primary_collection_t* const collection) {
  TRI_key_generator_t* generator;
  TRI_json_t* options;
  int res;

  generator = CreateGenerator(parameters, collection);
  if (generator == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  options = NULL;
  if (parameters != NULL) {
    options = TRI_LookupArrayJson(parameters, "options");
    if (options != NULL && options->_type != TRI_JSON_ARRAY) {
      options = NULL;
    }
  }

  res = generator->init(generator, options);
  if (res == TRI_ERROR_NO_ERROR) {
    collection->_keyGenerator = generator;
  }
  else {
    TRI_FreeKeyGenerator(generator);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a key generator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeKeyGenerator (TRI_key_generator_t* generator) {
  if (generator->free != NULL) {
    generator->free(generator);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
