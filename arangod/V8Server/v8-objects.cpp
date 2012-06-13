////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-objects.h"

#include "BasicsC/string-buffer.h"
#include "Basics/StringUtils.h"
#include "V8/v8-conv.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                              CONVERSION FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Conversions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts identifier into a object reference
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ObjectReference (TRI_voc_cid_t cid, TRI_voc_did_t did) {
  v8::HandleScope scope;
  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer, TRI_CORE_MEM_ZONE);
  TRI_AppendUInt64StringBuffer(&buffer, cid);
  TRI_AppendCharStringBuffer(&buffer, TRI_DOCUMENT_HANDLE_SEPARATOR_CHR);
  TRI_AppendUInt64StringBuffer(&buffer, did);

  v8::Handle<v8::String> ref = v8::String::New(buffer._buffer);

  TRI_AnnihilateStringBuffer(&buffer);

  return scope.Close(ref);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extratcs identifiers from a object reference
////////////////////////////////////////////////////////////////////////////////

bool TRI_IdentifiersObjectReference (v8::Handle<v8::Value> value, TRI_voc_cid_t& cid, TRI_voc_did_t& did) {
  bool error;

  cid = 0;
  did = 0;

  if (value->IsNumber() || value->IsNumberObject()) {
    did = (TRI_voc_did_t) TRI_ObjectToDouble(value, error);
    return ! error;
  }

  string v = TRI_ObjectToString(value);

  vector<string> doc = StringUtils::split(v, TRI_DOCUMENT_HANDLE_SEPARATOR_STR);

  switch (doc.size()) {
    case 1:
      did = StringUtils::uint64(doc[1]);
      return did != 0;

    case 2:
      cid = StringUtils::uint64(doc[0]);
      did = StringUtils::uint64(doc[1]);
      return cid != 0 && did != 0;

    default:
      return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
