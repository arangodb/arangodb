////////////////////////////////////////////////////////////////////////////////
/// @brief Avocado Simple Query Language
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var SQ = require("simple-query-basics");

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY ALL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SQ.SimpleQueryAll.prototype.execute = function () {
  var documents;

  if (this._execution == null) {
    if (this._skip == null || this._skip <= 0) {
      this._skip = 0;
    }

    documents = this._collection.ALL(this._skip, this._limit);

    this._execution = new SQ.GeneralArrayCursor(documents.documents);
    this._countQuery = documents.count;
    this._countTotal = documents.total;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  QUERY BY EXAMPLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-example
////////////////////////////////////////////////////////////////////////////////

SQ.SimpleQueryByExample.prototype.execute = function () {
  var documents;

  if (this._execution == null) {
    if (this._skip == null || this._skip <= 0) {
      this._skip = 0;
    }

    var parameters = [];

    // the actual example is passed in the first argument
    if (this._example.length == 1) {
      for (var i in this._example[0]) {
        if (this._example[0].hasOwnProperty(i)) {

          // attribute name
          parameters.push(i);

          // attribute value
          parameters.push(this._example[0][i]);
        }
      }
    }

    // the arguments are passed
    else {
      parameters = this._example;
    }

    var documents = this._collection.BY_EXAMPLE.apply(this._collection, parameters);

    this._execution = new SQ.GeneralArrayCursor(documents, this._skip, this._limit);
    this._countQuery = documents.length;
    this._countTotal = documents.length;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE QUERY NEAR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SQ.SimpleQueryNear.prototype.execute = function () {
  var result;
  var documents;
  var distances;
  var limit;

  if (this._execution == null) {
    if (this._skip == null || this._skip <= 0) {
      this._skip = 0;
    }

    if (this._limit == null) {
      limit = this._skip + 100;
    }
    else {
      limit = this._skip + this._limit;
    }

    result = this._collection.NEAR(this._index, this._latitude, this._longitude, limit);
    documents = result.documents;
    distances = result.distances;

    if (this._distance != null) {
      for (var i = this._skip;  i < documents.length;  ++i) {
        documents[i][this._distance] = distances[i];
      }
    }

    this._execution = new SQ.GeneralArrayCursor(result.documents, this._skip, null);
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               SIMPLE QUERY WITHIN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SQ.SimpleQueryWithin.prototype.execute = function () {
  var result;
  var documents;
  var distances;
  var limit;

  if (this._execution == null) {
    result = this._collection.WITHIN(this._index, this._latitude, this._longitude, this._radius);
    documents = result.documents;
    distances = result.distances;

    if (this._distance != null) {
      for (var i = this._skip;  i < documents.length;  ++i) {
        documents[i][this._distance] = distances[i];
      }
    }

    this._execution = new SQ.GeneralArrayCursor(result.documents, this._skip, this._limit);
    this._countQuery = result.documents.length - this._skip;
    this._countTotal = result.documents.length;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.SimpleQueryAll = SQ.SimpleQueryAll;
exports.SimpleQueryByExample = SQ.SimpleQueryByExample;
exports.SimpleQueryGeo = SQ.SimpleQueryGeo;
exports.SimpleQueryNear = SQ.SimpleQueryNear;
exports.SimpleQueryWithin = SQ.SimpleQueryWithin;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
