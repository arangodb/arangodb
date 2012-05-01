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

function getCursorResult (cursor) {
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
    result["id"] = cursorId;
  }
    
  if (hasCount) {
    result["count"] = count;
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor and return the first results
////////////////////////////////////////////////////////////////////////////////

function POST_api_cursor(req, res) {
  if (req.suffix.length != 0) {
    actions.resultNotFound(req, res, actions.ERROR_HTTP_NOT_FOUND);
    return;
  }

  try {
    var json = JSON.parse(req.requestBody);
      
    if (!json || !(json instanceof Object)) {
      actions.resultBad(req, res, actions.ERROR_QUERY_SPECIFICATION_INVALID, actions.getErrorMessage(actions.ERROR_QUERY_SPECIFICATION_INVALID));
      return;
    }

    var cursor;
    if (json.query != undefined) {
      cursor = AHUACATL_RUN(json.query, 
                            json.bindVars, 
                            (json.count != undefined ? json.count : false), 
                            (json.batchSize != undefined ? json.batchSize : 1000));  
    }
    else {
      actions.resultBad(req, res, actions.ERROR_QUERY_SPECIFICATION_INVALID, actions.getErrorMessage(actions.ERROR_QUERY_SPECIFICATION_INVALID));
      return;
    }
   
    if (cursor instanceof AvocadoError) {
      // error occurred
      actions.resultBad(req, res, cursor.errorNum, cursor.errorMessage);
      return;
    }

    // this might dispose or persist the cursor
    var result = getCursorResult(cursor);

    actions.resultOk(req, res, actions.HTTP_CREATED, result);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next results from an existing cursor
///
/// @REST{PUT /_api/cursor/@FA{cursor-identifier}}
///
/// If the cursor is still alive, returns an object with the following
/// attributes.
///
/// - @LIT{id}: the @FA{cursor-identifier}
/// - @LIT{result}: a list of documents for the current batch
/// - @LIT{hasMore}: @LIT{false} if this was the last batch
/// - @LIT{count}: if present the total number of elements
///
/// Note that even if @LIT{hasMore} returns @LIT{true}, the next call might
/// still return no documents. If, however, @LIT{hasMore} is @LIT{false}, then
/// the cursor is exhausted.
///
/// An @LIT{HTTP 404} is returned if the cursor no longer exists.
////////////////////////////////////////////////////////////////////////////////

function PUT_api_cursor(req, res) {
  if (req.suffix.length != 1) {
    actions.resultNotFound(req, res, actions.ERROR_HTTP_NOT_FOUND);
    return;
  }

  try {
    var cursorId = decodeURIComponent(req.suffix[0]); 
    var cursor = CURSOR(cursorId);

    if (!(cursor instanceof AvocadoCursor)) {
      actions.resultBad(req, res, actions.ERROR_CURSOR_NOT_FOUND, actions.getErrorMessage(actions.ERROR_CURSOR_NOT_FOUND));
      return;
    } 

    // note: this might dispose or persist the cursor
    actions.resultOk(req, res, actions.HTTP_OK, getCursorResult(cursor));
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dispose an existing cursor
///
/// @REST{DELETE /_api/cursor/@FA{cursor-identifier}}
///
/// Deletes the cursor and frees the resources associated with it.
////////////////////////////////////////////////////////////////////////////////

function DELETE_api_cursor(req, res) {
  if (req.suffix.length != 1) {
    actions.resultNotFound(req, res, actions.ERROR_HTTP_NOT_FOUND);
    return;
  }

  try {
    var cursorId = decodeURIComponent(req.suffix[0]);
    var cursor = CURSOR(cursorId);

    if (!(cursor instanceof AvocadoCursor)) {
      actions.resultBad(req, res, actions.ERROR_CURSOR_NOT_FOUND, actions.getErrorMessage(actions.ERROR_CURSOR_NOT_FOUND));
      return;
    }

    cursor.dispose();
    actions.resultOk(req, res, actions.HTTP_ACCEPTED, { "id" : cursorId });                
  }
  catch (err) {
    actions.resultException(req, res, err);
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
      case (actions.POST) : 
        POST_api_cursor(req, res); 
        break;

      case (actions.PUT) :  
        PUT_api_cursor(req, res); 
        break;

      case (actions.DELETE) :  
        DELETE_api_cursor(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
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
