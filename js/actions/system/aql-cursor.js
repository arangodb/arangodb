////////////////////////////////////////////////////////////////////////////////
/// @brief query results cursor actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

var actions = require("actions");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result set from a cursor
////////////////////////////////////////////////////////////////////////////////

function getCursorResult(cursor) {
  var hasCount = cursor.hasCount();
  var count = cursor.count();
  var rows = cursor.getRows();

  // must come after getRows()
  var hasNext = cursor.hasNext();
  var cursorId = null;
   
  if (hasNext) {
    cursor.persist();
    cursorId = cursor.id(); 
  }
  else {
    cursor.dispose();
  }

  var result = { 
    "result" : rows,
    "hasMore" : hasNext
  };

  if (cursorId) {
    result["_id"] = cursorId;
  }
    
  if (hasCount) {
    result["count"] = count;
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor and return the first results
////////////////////////////////////////////////////////////////////////////////

function postCursor(req, res) {
  if (req.suffix.length != 0) {
    actions.actionResultError (req, res, 404, actions.errorInvalidRequest, "Invalid request");
    return;
  }

  try {
    var json = JSON.parse(req.requestBody);
      
    if (!json || !(json instanceof Object)) {
      actions.actionResultError (req, res, 400, actions.errorQuerySpecificationInvalid, "Query specification invalid");
      return;
    }

    var cursor;
    if (json.query != undefined) {
      cursor = AQL_STATEMENT(json.query, 
                             json.bindVars, 
                             (json.count != undefined ? json.count : false), 
                             (json.batchSize != undefined ? json.batchSize : 1000));  
    }
    else {
      actions.actionResultError (req, res, 400, actions.errorQuerySpecificationInvalid, "Query specification invalid");
      return;
    }
   
    if (cursor instanceof AvocadoQueryError) {
      // error occurred
      actions.actionResultError (req, res, 404, cursor.code, cursor.message);
      return;
    }

    // this might dispose or persist the cursor
    var result = getCursorResult(cursor);

    actions.actionResultOK(req, res, 201, result);
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.errorJavascriptException, "Javascript exception");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next results from an existing cursor
////////////////////////////////////////////////////////////////////////////////

function putCursor(req, res) {
  if (req.suffix.length != 1) {
    actions.actionResultError (req, res, 404, actions.errorInvalidRequest, "Invalid request");
    return;
  }

  try {
    var cursorId = decodeURIComponent(req.suffix[0]); 
    var cursor = AQL_CURSOR(cursorId);
    if (!(cursor instanceof AvocadoQueryCursor)) {
      throw "cursor not found";
    } 

    // note: this might dispose or persist the cursor
    actions.actionResultOK(req, res, 200, getCursorResult(cursor));
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.errorCursorNotFound, "Cursor not found");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dispose an existing cursor
////////////////////////////////////////////////////////////////////////////////

function deleteCursor(req, res) {
  if (req.suffix.length != 1) {
    actions.actionResultError (req, res, 404, actions.errorInvalidRequest, "Invalid request");
    return;
  }

  try {
    var cursorId = decodeURIComponent(req.suffix[0]);
    var cursor = AQL_CURSOR(cursorId);
    if (!(cursor instanceof AvocadoQueryCursor)) {
      throw "cursor not found";
    }

    cursor.dispose();
    actions.actionResultOK(req, res, 202, { "_id" : cursorId });                
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.errorCursorNotFound, "Cursor not found");
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/cursor",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case ("POST") : 
        postCursor(req, res); 
        break;

      case ("PUT") :  
        putCursor(req, res); 
        break;

      case ("DELETE") :  
        deleteCursor(req, res); 
        break;

      default:
        actions.actionResultUnsupported(req, res);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
