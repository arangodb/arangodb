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
  var lines = cursor.getRows();
   
  var result = { 
    "result" : lines,
    "_id" : cursor.id(),
    "hasMore" : cursor.hasNext()
  };
    
  if (hasCount) {
    result["count"] = cursor.count();
  }
  
  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor and return the first results
////////////////////////////////////////////////////////////////////////////////

function postCursor(req, res) {
  if (req.suffix.length != 0) {
    actions.actionResultError (req, res, 404, actions.cursorNotModified, "Cursor not created"); 
    return;
  }

  try {
    var json = JSON.parse(req.requestBody);
    var queryString;

    if (json.qid != undefined) {
      var q = db.query.document_wrapped(json.qid);
      queryString = q.query;
    }    
    else if (json.query != undefined) {
      queryString = json.query;
    }

    if (queryString == undefined) {
      actions.actionResultError (req, res, 404, actions.cursorNotModified, "Missing query identifier");
      return;
    }
   
    var result;
    if (json.bindVars) {
      result = AQL_PREPARE(db, queryString, json.bindVars);
    }
    else {
      result = AQL_PREPARE(db, queryString);
    }

    if (result instanceof AvocadoQueryError) {
      actions.actionResultError (req, res, 404, result.code, result.message);
      return;
    }

    var cursor = result.execute((json.count != undefined ? json.count : false), (json.maxResults != undefined ? json.maxResults : 1000));
    if (cursor instanceof AvocadoQueryError) {
      actionResultError (req, res, 404, result.code, result.message);
      return;
    }

    actions.actionResultOK(req, res, 201, getCursorResult(cursor));
    cursor = null; 
    result = null;
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.cursorNotModified, "Cursor not created");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next results from an existing cursor
////////////////////////////////////////////////////////////////////////////////

function putCursor(req, res) {
  if (req.suffix.length != 1) {
    actions.actionResultError (req, res, 404, actions.cursorNotFound, "Cursor not found");
    return;
  }

  try {
    var cursorId = decodeURIComponent(req.suffix[0]); 
    var cursor = AQL_CURSOR(db, cursorId);
    if (!(cursor instanceof AvocadoQueryCursor)) {
      throw "cursor not found";
    } 

    actions.actionResultOK(req, res, 200, getCursorResult(cursor));
    cursor = null; 
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.cursorNotFound, "Cursor not found");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dispose an existing cursor
////////////////////////////////////////////////////////////////////////////////

function deleteCursor(req, res) {
  if (req.suffix.length != 1) {
    actions.actionResultError (req, res, 404, actions.cursorNotFound, "Cursor not found");
    return;
  }

  try {
    var cursorId = decodeURIComponent(req.suffix[0]);
    var cursor = AQL_CURSOR(db, cursorId);
    if (!(cursor instanceof AvocadoQueryCursor)) {
      throw "cursor not found";
    }

    cursor.dispose();
    actions.actionResultOK(req, res, 202, { "_id" : cursorId });                
    cursor = null; 
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.cursorNotFound, "Cursor not found");
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
