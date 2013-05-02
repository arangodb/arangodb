/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, unparam: true */
/*global require, exports*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A TODO-List Foxx-Application written for ArangoDB
///
/// @file This Document represents the repository communicating with ArangoDB
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {
  "use strict";
  
  var _ = require("underscore"),
    Foxx = require("org/arangodb/foxx"),
    structures = require("internal").db._collection("_structures"); // TODO!
  exports.Repository = Foxx.Repository.extend({
    // Define the save functionality
    getMonitored: function () {
      return [{
          name: "String"
        },{
          name: "Number"
        }];
      /*
      var a = structures.all(),
        res = [];
      while (a.hasNext()) {
        res.push({
          name: a.next().collection
        });
      }
      return res;
      */
    },
    
    getStructs: function (name) {
      switch(name) {
        case "String":
          return {
            attributes: {
              name : {
                type: "string"
              }
            }
          };
          break;
        case "Number":
          return {
            attributes: {
              int : {
                type: "number"
              },
              double: {
                type: "number"
              },
              boolean: {
                type: "boolean"
              }
            }
          };
          break;
        case "All":
          return {
            attributes: {
              int : {
                type: "number"
              },
              double: {
                type: "number"
              },
              name : {
                type: "string"
              },
              boolean: {
                type: "boolean"
              }
            }
          };
          break;
        default:
          res.json("Error has to be done!");
      }
      /*
      return structures.firstExample({collection: name});
      */
    },
    
    getContent: function (name) {
      switch(name) {
        case "String":
          return [{
            _id: "String/1",
            _rev: "1",
            _key: "1",
            name: "Test"
          }];
        case "Number":
          return [{
            _id: "Number/1",
            _rev: "1",
            _key: "1",
            int: 4,
            double: 4.5,
            boolean: true
          }];
        case "All":
          return [{
            _id: "All/1",
            _rev: "1",
            _key: "1",
            int: 4,
            double: 4.5,
            name: "Test",
            boolean: true
          }];
        default:
          return "Error has to be done!";
      }
      /*
      var db = require("internal").db._collection(name);
      // TODO: Requires Optimization!
      return db.toArray();
      */
    }
  });
  
}());
