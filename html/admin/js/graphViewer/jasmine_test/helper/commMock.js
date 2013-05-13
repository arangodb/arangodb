/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, window*/

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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

window.communicationMock = function (spyOn) {
  "use strict";
  
  var graph = {
    0: {"_id":0,"children" : [{"_id":1}, {"_id":2}, {"_id":3}, {"_id":4}]},
    1: {"_id": 1, "children": [{"_id": 5}, {"_id": 6}, {"_id": 7}]},
    2: {"_id": 2, "children": [{"_id": 8}]},
    3: {"_id": 3, "children": [{"_id": 8}, {"_id": 9}]},
    4: {"_id": 4, "children": [{"_id": 5}, {"_id": 12}]},
    5: {"_id": 5, "children": [{"_id": 10}, {"_id": 11}]},
    6: {"_id": 6, "children": []},
    7: {"_id": 7, "children": []},
    8: {"_id": 8, "children": []},
    9: {"_id": 9, "children": []},
    10: {"_id": 10, "children": []},
    11: {"_id": 11, "children": []},
    12: {"_id": 12, "children": []},
    42: {"_id": 42, "children": [{"_id": 43}, {"_id": 44}, {"_id": 45}]},
    43: {"_id": 43, "children": []},
    44: {"_id": 44, "children": []},
    45: {"_id": 45, "children": []},
    1337: {
      "_id": 1337,
      "children": [],
      "name": "Alice",
      "age": 42
    }
  };
  spyOn(d3, "json").andCallFake(function(path, cb) {
    var last = path.substring(path.lastIndexOf("/") + 1),
     obj, res; 
    if (last.substring(last.indexOf(".")) !== ".json") {
      cb("No JSON Object could be loaded");
      return;
    }
    obj = last.substring(0, last.indexOf("."));
    cb(null, graph[obj]);
  });    
};