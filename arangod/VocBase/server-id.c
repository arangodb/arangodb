////////////////////////////////////////////////////////////////////////////////
/// @brief server id handling
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
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "server-id.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/random.h"
#include "BasicsC/strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief the server's global id
////////////////////////////////////////////////////////////////////////////////

static TRI_server_id_t ServerId;

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
/// @brief initialise the global server id to 0 on startup
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseServerId () {
  memset(&ServerId, 0, sizeof(TRI_server_id_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the global server id
////////////////////////////////////////////////////////////////////////////////

TRI_server_id_t TRI_GetServerId () {
  return ServerId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief establish the global server id
////////////////////////////////////////////////////////////////////////////////

void TRI_EstablishServerId (const TRI_server_id_t id) {
  ServerId = id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads server id from file
////////////////////////////////////////////////////////////////////////////////

int TRI_ReadServerId (char const* filename) {
  TRI_json_t* json;
  TRI_json_t* idString;
  TRI_server_id_t foundId;

  assert(filename != NULL);

  if (! TRI_ExistsFile(filename)) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  json = TRI_JsonFile(TRI_UNKNOWN_MEM_ZONE, filename, NULL);
  if (json == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  idString = TRI_LookupArrayJson(json, "serverId");

  if (idString == NULL || idString->_type != TRI_JSON_STRING) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return TRI_ERROR_INTERNAL;
  }

  foundId = TRI_UInt64String(idString->_value._string.data);
  LOG_INFO("using existing server id: %llu", (unsigned long long) foundId);

  if (foundId == 0) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return TRI_ERROR_INTERNAL;
  }

  TRI_EstablishServerId(foundId);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes server id to file
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteServerId (char const* filename) {
  TRI_json_t* json;
  char* idString;
  char buffer[32];
  size_t len;
  time_t tt;
  struct tm tb;
  bool ok;

  assert(filename != NULL);

  // create a json object
  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (json == NULL) {
    // out of memory
    LOG_ERROR("cannot save server id in file '%s': out of memory", filename);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  assert(ServerId != 0);

  idString = TRI_StringUInt64((uint64_t) ServerId);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "serverId", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, idString));
  TRI_FreeString(TRI_CORE_MEM_ZONE, idString);

  tt = time(0);
  TRI_gmtime(tt, &tb);
  len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "createdTime", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, buffer, len));

  // save json info to file
  LOG_DEBUG("Writing server id to file '%s'", filename);
  ok = TRI_SaveJson(filename, json);

  if (! ok) {
    LOG_ERROR("could not save shutdown information in file '%s': %s", filename, TRI_last_error());
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    return TRI_ERROR_INTERNAL;
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a new server id
///
/// TODO: generate a real UUID instead of the 2 random values
////////////////////////////////////////////////////////////////////////////////

int TRI_GenerateServerId () {
  uint64_t randomValue = 0ULL; // init for our friend Valgrind
  uint32_t* value;

  // save two uint32_t values
  value = (uint32_t*) &randomValue;
  *(value++) = TRI_UInt32Random();
  *(value)   = TRI_UInt32Random();

  // use the lower 6 bytes only
  randomValue &= TRI_SERVER_ID_MASK;

  TRI_EstablishServerId((TRI_server_id_t) randomValue);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
