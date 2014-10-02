////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase database defaults
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "vocbase-defaults.h"
#include "Basics/json.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief apply default settings
////////////////////////////////////////////////////////////////////////////////

void TRI_ApplyVocBaseDefaults (TRI_vocbase_t* vocbase,
                               TRI_vocbase_defaults_t const* defaults) {
  vocbase->_settings.defaultMaximalSize               = defaults->defaultMaximalSize;
  vocbase->_settings.defaultWaitForSync               = defaults->defaultWaitForSync;
  vocbase->_settings.requireAuthentication            = defaults->requireAuthentication;
  vocbase->_settings.requireAuthenticationUnixSockets = defaults->requireAuthenticationUnixSockets;
  vocbase->_settings.authenticateSystemOnly           = defaults->authenticateSystemOnly;
  vocbase->_settings.forceSyncProperties              = defaults->forceSyncProperties;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert defaults into a JSON array
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonVocBaseDefaults (TRI_memory_zone_t* zone,
                                     TRI_vocbase_defaults_t const* defaults) {
  TRI_json_t* json = TRI_CreateArrayJson(zone);

  if (json == nullptr) {
    return nullptr;
  }

  TRI_Insert3ArrayJson(zone, json, "waitForSync", TRI_CreateBooleanJson(zone, defaults->defaultWaitForSync));
  TRI_Insert3ArrayJson(zone, json, "requireAuthentication", TRI_CreateBooleanJson(zone, defaults->requireAuthentication));
  TRI_Insert3ArrayJson(zone, json, "requireAuthenticationUnixSockets", TRI_CreateBooleanJson(zone, defaults->requireAuthenticationUnixSockets));
  TRI_Insert3ArrayJson(zone, json, "authenticateSystemOnly", TRI_CreateBooleanJson(zone, defaults->authenticateSystemOnly));
  TRI_Insert3ArrayJson(zone, json, "forceSyncProperties", TRI_CreateBooleanJson(zone, defaults->forceSyncProperties));
  TRI_Insert3ArrayJson(zone, json, "defaultMaximalSize", TRI_CreateNumberJson(zone, (double) defaults->defaultMaximalSize));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhance defaults with data from JSON
////////////////////////////////////////////////////////////////////////////////

void TRI_FromJsonVocBaseDefaults (TRI_vocbase_defaults_t* defaults,
                                  TRI_json_t const* json) {
  if (! TRI_IsArrayJson(json)) {
    return;
  }

  TRI_json_t* optionJson;
  optionJson = TRI_LookupArrayJson(json, "waitForSync");

  if (TRI_IsBooleanJson(optionJson)) {
    defaults->defaultWaitForSync = optionJson->_value._boolean;
  }

  optionJson = TRI_LookupArrayJson(json, "requireAuthentication");

  if (TRI_IsBooleanJson(optionJson)) {
    defaults->requireAuthentication = optionJson->_value._boolean;
  }

  optionJson = TRI_LookupArrayJson(json, "requireAuthenticationUnixSockets");

  if (TRI_IsBooleanJson(optionJson)) {
    defaults->requireAuthenticationUnixSockets = optionJson->_value._boolean;
  }

  optionJson = TRI_LookupArrayJson(json, "authenticateSystemOnly");

  if (TRI_IsBooleanJson(optionJson)) {
    defaults->authenticateSystemOnly = optionJson->_value._boolean;
  }
  
  optionJson = TRI_LookupArrayJson(json, "forceSyncProperties");

  if (TRI_IsBooleanJson(optionJson)) {
    defaults->forceSyncProperties = optionJson->_value._boolean;
  }

  optionJson = TRI_LookupArrayJson(json, "defaultMaximalSize");

  if (TRI_IsNumberJson(optionJson)) {
    defaults->defaultMaximalSize = (TRI_voc_size_t) optionJson->_value._number;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
