/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global _*/
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
function EventDispatcher(nodeShaper, edgeShaper, config) {
  "use strict";
  
  var eventlib,
  expandConfig,
  self = this;
  
  if (nodeShaper === undefined) {
    throw "NodeShaper has to be given.";
  }
  
  if (edgeShaper === undefined) {
    throw "EdgeShaper has to be given.";
  }
  
  eventlib = new EventLibrary();
  
  self.events = {};
  
  if (config !== undefined) {
    if (config.expand !== undefined) {
      if (eventlib.checkExpandConfig(config.expand)) {
        self.events.EXPAND = new eventlib.Expand(config.expand);
      }
    }
    if (config.nodeEditor !== undefined) {
      if (config.nodeEditor.shaper === undefined) {
        config.nodeEditor.shaper = nodeShaper;
      }
      if (eventlib.checkNodeEditorConfig(config.nodeEditor)) {
        self.events.CREATENODE = new eventlib.InsertNode(config.nodeEditor);
        self.events.PATCHNODE = new eventlib.PatchNode(config.nodeEditor);
        self.events.DELETENODE = new eventlib.DeleteNode(config.nodeEditor);
      }
    }
    if (config.edgeEditor !== undefined) {
      if (config.edgeEditor.shaper === undefined) {
        config.edgeEditor.shaper = edgeShaper;
      }
      if (eventlib.checkEdgeEditorConfig(config.edgeEditor)) {
        self.events.CREATEEDGE = new eventlib.InsertEdge(config.edgeEditor);
        self.events.PATCHEDGE = new eventlib.PatchEdge(config.edgeEditor);
        self.events.DELETEEDGE = new eventlib.DeleteEdge(config.edgeEditor);
      }
    }
  }
  Object.freeze(self.events);
  
  //Check for expand config
  self.bind = function (object, event, func) {
    if (func === undefined || !_.isFunction(func)) {
      throw "You have to give a function that should be bound as a third argument";
    }
    switch (object) {
      case "nodes":
        nodeShaper.on(event, func);
        nodeShaper.drawNodes();
        break;
      case "edges":
        edgeShaper.on(event, func);
        edgeShaper.drawEdges();
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