/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global _, $*/
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
function EventDispatcher(nodeShaper, edgeShaper, config) {
  "use strict";
  
  var eventlib,
    expandConfig,
    svgBase,
    svgTemp,
    svgObj,
    self = this,
    parseNodeEditorConfig = function(config) {
      if (config.shaper === undefined) {
        config.shaper = nodeShaper;
      }
      if (eventlib.checkNodeEditorConfig(config)) {
        var insert = new eventlib.InsertNode(config),
          patch = new eventlib.PatchNode(config),
          del = new eventlib.DeleteNode(config);
        
        self.events.CREATENODE = function(callback) {
          return function() {
            insert(callback);
          };
        };
        self.events.PATCHNODE = function(node, getNewData, callback) {
          if (!_.isFunction(getNewData)) {
            throw "Please give a function to extract the new node data";
          }
          return function() {
            patch(node, getNewData(), callback);
          };
        };
        
        self.events.DELETENODE = function(callback) {
          return function(node) {
            del(node, callback);
          };
        };
      }
    },
    
    parseEdgeEditorConfig = function(config) {
      if (config.shaper === undefined) {
        config.shaper = edgeShaper;
      }
      if (eventlib.checkEdgeEditorConfig(config)) {
        var insert = new eventlib.InsertEdge(config),
          patch = new eventlib.PatchEdge(config),
          del = new eventlib.DeleteEdge(config),
          edgeStart = null,
          didInsert = false;
        
        self.events.STARTCREATEEDGE = function(callback) {
          return function(node) {
            edgeStart = node;
            didInsert = false;
            if (callback !== undefined) {
              callback();
            }
          };
        };
        
        self.events.CANCELCREATEEDGE = function(callback) {
          return function() {
            edgeStart = null;
            if (callback !== undefined && !didInsert) {
              callback();
            }
          };
        };
        
        self.events.FINISHCREATEEDGE = function(callback) {
          return function(node) {
            if (edgeStart !== null && node !== edgeStart) {
              insert(edgeStart, node, callback);
              didInsert = true;
            }
          };
        };
        
        self.events.PATCHEDGE = function(edge, getNewData, callback) {
          if (!_.isFunction(getNewData)) {
            throw "Please give a function to extract the new node data";
          }
          return function() {
            patch(edge, getNewData(), callback);
          };
        };
        
        self.events.DELETEEDGE = function(callback) {
          return function(edge) {
            del(edge, callback);
          };
        };
      }
    },
    
    bindSVGEvents = function() {
      svgObj = svgObj || $("svg");
      _.each(svgBase, function(fs, ev) {
        svgObj.bind(ev, function(trigger) {
          _.each(fs, function(f) {
            f(trigger);
          });
          if (!! svgTemp[ev]) {
            svgTemp[ev](trigger);
          }
        });
      });
    };
  
  if (nodeShaper === undefined) {
    throw "NodeShaper has to be given.";
  }
  
  if (edgeShaper === undefined) {
    throw "EdgeShaper has to be given.";
  }
  
  eventlib = new EventLibrary();
  
  svgBase = {
    click: [],
    dblclick: [],
    mousedown: [],
    mouseup: [],
    mousemove: []
  };
  svgTemp = {};
  
  self.events = {};
  
  if (config !== undefined) {
    if (config.expand !== undefined) {
      if (eventlib.checkExpandConfig(config.expand)) {
        self.events.EXPAND = new eventlib.Expand(config.expand);
      }
    }
    if (config.drag !== undefined) {
      if (eventlib.checkDragConfig(config.drag)) {
        self.events.DRAG = new eventlib.Drag(config.drag);
      }
    }
    if (config.nodeEditor !== undefined) {
      parseNodeEditorConfig(config.nodeEditor);
    }
    if (config.edgeEditor !== undefined) {
      parseEdgeEditorConfig(config.edgeEditor);
    }
  }
  Object.freeze(self.events);
  
  //Check for expand config
  self.bind = function (object, event, func) {
    if (func === undefined || !_.isFunction(func)) {
      throw "You have to give a function that should be bound as a third argument";
    }
    var actions = {};
    switch (object) {
      case "nodes":
        actions[event] = func;
        nodeShaper.changeTo({
          actions: actions
        });
        break;
      case "edges":
        actions[event] = func;
        edgeShaper.changeTo({
          actions: actions
        });
        break;
      case "svg":
        svgTemp[event] = func;
        bindSVGEvents();
        break;
      default:
        if (object.bind !== undefined) {
          object.unbind(event);
          object.bind(event, func);
        } else {
          throw "Sorry cannot bind to object. Please give either "
          + "\"nodes\", \"edges\" or a jQuery-selected DOM-Element";
        }
    }
  };
  
  self.rebind = function (object, actions) {
    actions = actions || {};
    actions.reset = true;
    switch (object) {
      case "nodes":
        nodeShaper.changeTo({
          actions: actions
        });
        break;
      case "edges":
        edgeShaper.changeTo({
          actions: actions
        });
        break;
      case "svg":
        svgTemp = {};
        _.each(actions, function(fs, ev) {
          if (ev !== "reset") {
            svgTemp[ev] = fs;
          }
        });
        bindSVGEvents();
        break;
      default:
          throw "Sorry cannot rebind to object. Please give either "
          + "\"nodes\", \"edges\" or \"svg\"";
    }
  };
  
  self.fixSVG = function(event, action) {
    if (svgBase[event] === undefined) {
      throw "Sorry unkown event";
    }
    svgBase[event].push(action);
    bindSVGEvents();
  };
  /*
  self.unbind = function () {
    throw "Not implemented";
  };
  */
}