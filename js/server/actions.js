////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var time = SYS_TIME;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json V8 JSON
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as documents
///
/// @FUN{queryResult(@FA{req}, @FA{res}, @FA{query})}
///
/// The functions returns the result of a query using pagination. It assumes
/// that the request has the numerical parameters @LIT{blocksize} and @LIT{page}
/// or @LIT{offset}. @LIT{blocksize} determines the maximal number of result
/// documents to return. You can either specify @LIT{page} or @LIT{offset}.
/// @LIT{page} is the number of pages to skip, i. e. @LIT{page} *
/// @LIT{blocksize} documents. @LIT{offset} is the number of documents to skip.
///
/// If you are using pagination, than the query must be a sorted query.
////////////////////////////////////////////////////////////////////////////////

function queryResult (req, res, query) {
  var result;
  var offset = 0;
  var page = 0;
  var blocksize = 0;
  var t;
  var result;

  t = time();

  if (req.blocksize) {
    blocksize = req.blocksize;

    if (req.page) {
      page = req.page;
      offset = page * blocksize;
      query = query.skip(offset).limit(blocksize);
    }
    else {
      query = query.limit(blocksize);
    }
  }

  result = query.toArray();

  result = {
    total : query.count(),
    count : query.count(true),
    offset : offset,
    blocksize : blocksize,
    page : page,
    runtime : time() - t,
    documents : result
  };

  res.responseCode = 200;
  res.contentType = "application/json";
  res.body = JSON.stringify(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as references
///
/// @FUN{queryReferences(@FA{req}, @FA{res}, @FA{query})}
///
/// The methods works like @FN{queryResult} but instead of the documents only
/// document identifiers are returned.
////////////////////////////////////////////////////////////////////////////////

function queryReferences (req, res, query) {
  var result;
  var offset = 0;
  var page = 0;
  var blocksize = 0;
  var t;
  var result;

  t = time();

  if (req.blocksize) {
    blocksize = req.blocksize;

    if (req.page) {
      page = req.page;
      offset = page * blocksize;
      query = query.skip(offset).limit(blocksize);
    }
    else {
      query = query.limit(blocksize);
    }
  }

  result = [];

  while (query.hasNext()) {
    result.push(query.nextRef());
  }

  result = {
    total : query.count(),
    count : query.count(true),
    offset : offset,
    blocksize : blocksize,
    page : page,
    runtime : time() - t,
    references : result
  };

  res.responseCode = 200;
  res.contentType = "application/json";
  res.body = JSON.stringify(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result
///
/// @FUN{actionResult(@FA{req}, @FA{res}, @FA{code}, @FA{result})}
///
/// The functions returns a result object. @FA{code} is the status code
/// to return.
////////////////////////////////////////////////////////////////////////////////

function actionResult (req, res, code, result) {
  if (code == 204) {
    res.responseCode = code;
  }
  else {
    res.responseCode = code;
    res.contentType = "application/json";
    res.body = JSON.stringify(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an error
///
/// @FUN{actionError(@FA{req}, @FA{res}, @FA{errorMessage})}
///
/// The functions returns an error message. The status code is 500 and the
/// returned object is an array with an attribute @LIT{error} containing
/// the error message @FA{errorMessage}.
////////////////////////////////////////////////////////////////////////////////

function actionError (req, res, err) {
  res.responseCode = 500;
  res.contentType = "application/json";
  res.body = JSON.stringify({ 'error' : "" + err });
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
