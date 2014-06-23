////////////////////////////////////////////////////////////////////////////////
/// @brief static transaction helper functions
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

#include "Helper.h"
#include "Transaction/transactions.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::transaction;

////////////////////////////////////////////////////////////////////////////////
/// @brief append a document key
////////////////////////////////////////////////////////////////////////////////

bool Helper::appendKey (Bson& document,
                        string const& key) {
  return document.appendUtf8(string(TRI_VOC_ATTRIBUTE_KEY), key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a document key
/// this will throw if the document key is invalid
/// if the document does not contain the _key attribute, an empty string will
/// be returned
////////////////////////////////////////////////////////////////////////////////

string Helper::documentKey (Bson const& document) {
  BsonIter iter(document);

  if (! iter.find(TRI_VOC_ATTRIBUTE_KEY)) {
    // no _key attribute
    return "";
  }

  // document has _key attribute
  if (iter.getType() != BSON_TYPE_UTF8) {
    // _key has an invalid type
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }

  // _key is a string
  string const key = iter.getUtf8();

  if (key.empty()) {
    // _key is empty
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }

  return key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a BSON document from the string
////////////////////////////////////////////////////////////////////////////////

Bson Helper::documentFromJson (string const& data) {
  return documentFromJson(data.c_str(), data.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a BSON document from the string
////////////////////////////////////////////////////////////////////////////////

Bson Helper::documentFromJson (char const* data,
                               size_t length) {
  Bson document;

  if (! document.fromJson(data)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  return document;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
