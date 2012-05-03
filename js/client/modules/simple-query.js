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
    var data = {
      collection : this._collection._id
    }  

    if (this._limit != null) {
      data.limit = this._limit;
    }

    if (this._skip != null) {
      data.skip = this._skip;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/all", JSON.stringify(data));

    TRI_CheckRequestResult(requestResult);

    this._execution = new AvocadoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
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

    var parameters = [ ];

    // the actual example is passed in the first argument
    for (var i in this._example[0]) {
      if (this._example[0].hasOwnProperty(i)) {

        // attribute name
        parameters.push(i);

        // attribute value
        parameters.push(this._example[0][i]);
      }
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
  var documents;

  if (this._execution == null) {
    var data = {
      collection : this._collection._id,
      latitude : this._latitude,
      longitude : this._longitude
    }  

    if (this._limit != null) {
      data.limit = this._limit;
    }

    if (this._skip != null) {
      data.skip = this._skip;
    }

    if (this._index != null) {
      data.geo = this._index;
    }
  
    if (this._distance != null) {
      data.distance = this._distance;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/near", JSON.stringify(data));

    TRI_CheckRequestResult(requestResult);

    this._execution = new AvocadoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
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
  var documents;

  if (this._execution == null) {
    var data = {
      collection : this._collection._id,
      latitude : this._latitude,
      longitude : this._longitude,
      radius : this._radius
    }  

    if (this._limit != null) {
      data.limit = this._limit;
    }

    if (this._skip != null) {
      data.skip = this._skip;
    }

    if (this._index != null) {
      data.geo = this._index;
    }
  
    if (this._distance != null) {
      data.distance = this._distance;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/within", JSON.stringify(data));

    TRI_CheckRequestResult(requestResult);

    this._execution = new AvocadoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
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
