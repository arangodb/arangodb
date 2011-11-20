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

// -----------------------------------------------------------------------------
// --SECTION--                                             actions output helper
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json V8 JSON
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as documents
////////////////////////////////////////////////////////////////////////////////

function queryResult (req, res, query) {
  var result;
  var offset = 0;
  var page = 0;
  var blocksize = 0;

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

  result = {
    total : query.count(),
    count : query.count(true),
    offset : offset,
    blocksize : blocksize,
    page : page,
    documents : query.toArray()
  };

  res.responseCode = 200;
  res.contentType = "application/json";
  res.body = toJson(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as references
////////////////////////////////////////////////////////////////////////////////

function queryReferences (req, res, query) {
  var result;
  var offset = 0;
  var page = 0;
  var blocksize = 0;

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

  result = {
    total : query.count(),
    count : query.count(true),
    offset : offset,
    blocksize : blocksize,
    page : page,
    references : query.toArray().map(function(doc) { return doc._id; })
  };

  res.responseCode = 200;
  res.contentType = "application/json";
  res.body = toJson(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
