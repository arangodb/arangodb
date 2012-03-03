////////////////////////////////////////////////////////////////////////////////
/// @brief demo actions
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

var actions = require("actions");

////////////////////////////////////////////////////////////////////////////////
/// @brief geo "near" query
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "geo/near",
  domain : "user",

  parameters : {
    collection : "collection",
    lat : "number",
    lon : "number"
  },

<<<<<<< HEAD
<<<<<<< HEAD
  callback : function (req, res) {
=======
  function : function (req, res) {
>>>>>>> unfinished actions cleanup
=======
  callback : function (req, res) {
>>>>>>> action cleanup
    var result = req.collection.near(req.lat, req.lon).distance();

    actions.queryResult(req, res, result);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief geo "within" query
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "within",
  domain : "user",

   parameters : {
     collection : "collection",
     lat : "number",
     lon : "number",
     radius : "number"
  },

<<<<<<< HEAD
<<<<<<< HEAD
  callback : function (req, res) {
=======
  function : function (req, res) {
>>>>>>> unfinished actions cleanup
=======
  callback : function (req, res) {
>>>>>>> action cleanup
    var result = req.collection.within(req.lat, req.lon, req.radius).distance();

    actions.queryResult(req, res, result);
  }
});

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
