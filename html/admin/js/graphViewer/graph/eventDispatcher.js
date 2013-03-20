/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global EventLibrary*/
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
function EventDispatcher(nodeShaper, edgeShaper, initialConfig) {
  "use strict";
  
  var eventlib,
  self = this;
  
  if (nodeShaper === undefined) {
    throw "NodeShaper has to be given.";
  }
  
  if (edgeShaper === undefined) {
    throw "EdgeShaper has to be given.";
  }
  
  eventlib = new EventLibrary();
  
  self.bind = function (object, event, func) {
    switch (object) {
      case "nodes":
        nodeShaper.on(event, func);
        break;
      case "edges":
        edgeShaper.on(event, func);
        break;
      default:
      if (object.bind !== undefined) {
        object.bind(event, func);
      } else {
        throw "Sorry cannot bind to object. Please give either "
        + "\"nodes\", \"edges\" or a jQuery-selected DOM-Element";
      }
    }
    
    
    
  };
  
  self.unbind = function () {
    throw "Not implemented";
  };
}

EventDispatcher.events = Object.freeze({
  EXPAND: 1,
  CREATENODE: 2,
  PATCHNODE: 3,
  DELETENODE: 4,
  CREATEEDGE: 5,
  PATCHEDGE: 6,
  DELETEEDGE: 7
});