////////////////////////////////////////////////////////////////////////////////
/// @brief import request handler
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestImportHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"
#include "Rest/HttpRequest.h"
#include "Rest/JsonContainer.h"
#include "VocBase/simple-collection.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestImportHandler::RestImportHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestImportHandler::queue () {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestImportHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  // prepare logging
  static LoggerData::Task const logCreate(DOCUMENT_IMPORT_PATH + " [create]");
  static LoggerData::Task const logIllegal(DOCUMENT_IMPORT_PATH + " [illegal]");

  LoggerData::Task const * task = &logCreate;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_POST: task = &logCreate; break;
    default: task = &logIllegal; break;
  }

  _timing << *task;
  LOGGER_REQUEST_IN_START_I(_timing);

  // execute one of the CRUD methods
  bool res = false;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_POST: {
        // extract the import type
        bool found;
        string documentType = _request->value("type", found);
        
        if (found && documentType == "documents") {
          res = createByArray();  
        }
        else {
          res = createByList();           
        }      
      }
      break;
    default:
      res = false;
      generateNotImplemented("ILLEGAL " + DOCUMENT_IMPORT_PATH);
      break;
  }

  _timingResult = res ? RES_ERR : RES_OK;

  // this handler is done
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates documents
///
/// @REST{POST /_api/import?type=documents&collection=@FA{collection-identifier}}
///
/// Creates documents in the collection identified by the
/// @FA{collection-identifier}.  The JSON representations of the documents must 
/// be passed as the body of the POST request.
///
/// If the document was created successfully, then a @LIT{HTTP 201} is returned.
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createByArray () {
  size_t numCreated = 0;
  size_t numError = 0;
  size_t numEmpty = 0;
  
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_IMPORT_PATH + "?type=documents&collection=<identifier>");
    return false;
  }

  // extract the collection name
  bool found;
  string collection = _request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }
  
  // shall we create the collection?
  char const* valueStr = _request->value("createCollection", found);
  bool create = found ? StringUtils::boolean(valueStr) : false;
  
  // shall we reuse document and revision id?
  valueStr = _request->value("useId", found);
  bool reuseId = found ? StringUtils::boolean(valueStr) : false;

  // find and load collection given by name or identifier
  int res = useCollection(collection, create);

  if (res != TRI_ERROR_NO_ERROR) {
    releaseCollection();
   
    // error is already generated by useCollection! 
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  size_t start = 0;
  size_t next = 0;
  string line;
  
  string body(_request->body(), _request->bodySize());

  while (next != string::npos && start < body.size()) {
    next = body.find('\n', start);

    if (next == string::npos) {
      line = body.substr(start);
    }
    else {
      line = body.substr(start, next - start);
      start = next + 1;      
    }

    StringUtils::trimInPlace(line, "\r\n\t ");
    if (line.length() == 0) {
      ++numEmpty;
      continue;
    }
    
    //LOGGER_TRACE << "line = " << line;
    
    TRI_json_t* values = parseJsonLine(line);

    if (values) {      
      // now save the document
      TRI_doc_mptr_t const mptr = _documentCollection->createJson(_documentCollection, TRI_DOC_MARKER_DOCUMENT, values, 0, reuseId, false);
      if (mptr._did != 0) {
        ++numCreated;
      }
      else {
        ++numError;
      }
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, values);
      
    }
    else {
      LOGGER_WARNING << "no valid JSON data in line " << line;            
      ++numError;
    }
            
  }

  _documentCollection->endWrite(_documentCollection);
  
  // .............................................................................
  // outside write transaction
  // .............................................................................

  // release collection and free json
  releaseCollection();

  // generate result
  generateDocumentsCreated(numCreated, numError, numEmpty);
  
  return true;
}




////////////////////////////////////////////////////////////////////////////////
/// @brief creates documents
///
/// @REST{POST /_api/import?collection=@FA{collection-identifier}}
///
/// Creates documents in the collection identified by the
/// @FA{collection-identifier}.  The JSON representations of the documents must 
/// be passed as the body of the POST request.
///
/// If the document was created successfully, then a @LIT{HTTP 201} is returned.
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createByList () {
  size_t numCreated = 0;
  size_t numError = 0;
  size_t numEmpty = 0;
  
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  // extract the collection name
  bool found;
  string collection = _request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }
  
  // shall we create the collection?
  char const* valueStr = _request->value("createCollection", found);
  bool create = found ? StringUtils::boolean(valueStr) : false;
  
  // shall we reuse document and revision id?
  valueStr = _request->value("useId", found);
  bool reuseId = found ? StringUtils::boolean(valueStr) : false;

  size_t start = 0;
  string body(_request->body(), _request->bodySize());
  size_t next = body.find('\n', start);
  
  if (next == string::npos) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON list in second line found");
    return false;            
  }
  
  TRI_json_t* keys = 0;
    
  string line = body.substr(start, next);
  StringUtils::trimInPlace(line, "\r\n\t ");

  // get first line
  if (line != "") { 
    
    keys = parseJsonLine(line);
    
    if (!keys) {
      LOGGER_WARNING << "no JSON data in first line";
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "no JSON string list in first line found");
      return false;      
    }
    
    if (keys->_type == TRI_JSON_LIST) {
      if (!checkKeys(keys)) {
        LOGGER_WARNING << "no JSON string list in first line found";
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
        generateError(HttpResponse::BAD,
                      TRI_ERROR_HTTP_BAD_PARAMETER,
                      "no JSON string list in first line found");
        return false;        
      }
      start = next + 1;
    }
    else {
      LOGGER_WARNING << "no JSON string list in first line found";
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "no JSON string list in first line found");
      return false;      
    }        
  }
  else {
    LOGGER_WARNING << "no JSON string list in first line found";
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string list in first line found");
    return false;      
  }        
      
  // find and load collection given by name or identifier
  int res = useCollection(collection, create);

  if (res != TRI_ERROR_NO_ERROR) {
    releaseCollection();
    
    if (keys) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
    }
 
    // error is already generated by useCollection! 
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  while (next != string::npos && start < body.length()) {
    next = body.find('\n', start);

    if (next == string::npos) {
      line = body.substr(start);
    }
    else {
      line = body.substr(start, next - start);
      start = next + 1;      
    }
    
    StringUtils::trimInPlace(line, "\r\n\t ");
    if (line.length() == 0) {
      ++numEmpty;
      continue;
    }
    
    //LOGGER_TRACE << "line = " << line;
    
    TRI_json_t* values = parseJsonLine(line);
    TRI_json_t* json = 0;

    if (values) {
      // got a json document or list
      
      // build the json object from the list
      json = createJsonObject(keys, values);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, values);
        
      if (!json) {
        LOGGER_WARNING << "no valid JSON data in line: " << line;            
        ++numError;          
        continue;
      }

      // now save the document
      TRI_doc_mptr_t const mptr = _documentCollection->createJson(_documentCollection, TRI_DOC_MARKER_DOCUMENT, json, 0, reuseId, false);
      if (mptr._did != 0) {
        ++numCreated;
      }
      else {
        ++numError;
      }
      
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);      
    }
    else {
      LOGGER_WARNING << "no valid JSON data in line: " << line;            
      ++numError;
    }
            
  }
  
  if (keys) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
  }

  _documentCollection->endWrite(_documentCollection);
  
  // .............................................................................
  // outside write transaction
  // .............................................................................

  // release collection and free json
  releaseCollection();

  // generate result
  generateDocumentsCreated(numCreated, numError, numEmpty);
  
  return true;
}



void RestImportHandler::generateDocumentsCreated (size_t numCreated, size_t numError, size_t numEmpty) {
  _response = new HttpResponse(HttpResponse::CREATED);

  _response->setContentType("application/json; charset=utf-8");

  _response->body()
    .appendText("{\"error\":false,\"created\":")
    .appendInteger(numCreated)
    .appendText(",\"errors\":")
    .appendInteger(numError)
    .appendText(",\"empty\":")
    .appendInteger(numEmpty)
    .appendText("}");
}

TRI_json_t* RestImportHandler::parseJsonLine (const string& line) {
  char* errmsg = 0;
  TRI_json_t* json = TRI_Json2String(TRI_UNKNOWN_MEM_ZONE, line.c_str(), &errmsg);

  if (errmsg != 0) {
    // must free this error message, otherwise we'll have a memleak
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, errmsg);
  }
  return json;
}

TRI_json_t* RestImportHandler::createJsonObject (TRI_json_t* keys, TRI_json_t* values) {
  
  if (values->_type != TRI_JSON_LIST) {
    return 0;
  }
  
  if (keys->_value._objects._length !=  values->_value._objects._length) {
    return 0;
  }
  
  TRI_json_t* result = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  
  size_t n = keys->_value._objects._length;

  for (size_t i = 0;  i < n;  ++i) {

    TRI_json_t* key   = (TRI_json_t*) TRI_AtVector(&keys->_value._objects, i);
    TRI_json_t* value = (TRI_json_t*) TRI_AtVector(&values->_value._objects, i);
    
    if (key->_type == TRI_JSON_STRING && value->_type > TRI_JSON_NULL) {
      TRI_InsertArrayJson(TRI_UNKNOWN_MEM_ZONE, result, key->_value._string.data, value);      
    }
  }  
  
  return result;
}

bool RestImportHandler::checkKeys (TRI_json_t* keys) {
  if (keys->_type != TRI_JSON_LIST) {
    return false;
  }
  
  size_t n = keys->_value._objects._length;
  
  if (n == 0) {
    return false;
  }

  for (size_t i = 0;  i < n;  ++i) {

    TRI_json_t* key   = (TRI_json_t*) TRI_AtVector(&keys->_value._objects, i);
    
    if (key->_type != TRI_JSON_STRING) {
      return false;
    }
  }  
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
