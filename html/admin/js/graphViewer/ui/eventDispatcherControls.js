/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document*/
/*global modalDialogHelper, uiComponentsHelper */
/*global EventDispatcher, EventLibrary*/
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
function EventDispatcherControls(list, nodeShaper, edgeShaper, dispatcherConfig) {
  "use strict";
  
  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (nodeShaper === undefined) {
    throw "The NodeShaper has to be given.";
  }
  if (edgeShaper === undefined) {
    throw "The EdgeShaper has to be given.";
  }
  var self = this,
    baseClass = "event",
    eventlib = new EventLibrary(),
    dispatcher = new EventDispatcher(nodeShaper, edgeShaper, dispatcherConfig),
    
    
    createButton = function(title, callback) {
      uiComponentsHelper.createButton(
        baseClass,
        list,
        title,
        "control_" + title,
        callback
      );
    },
    rebindNodes = function(actions) {
      actions = actions || {};
      actions.reset = true;
      nodeShaper.changeTo({
        actions: actions
      });
    },
    rebindEdges = function(actions) {
      actions = actions || {};
      actions.reset = true;
      edgeShaper.changeTo({
        actions: actions
      });
    }
  
  this.addControlDrag = function() {
    var prefix = "control_drag",
      idprefix = prefix + "_",
      callback = function() {
        rebindNodes( {
          drag: function() {console.log("Not implemented");}
        });
        rebindEdges();
        
        
      };
    createButton("drag", callback);
  };
  
  this.addControlEdit = function() {
    var prefix = "control_edit",
      idprefix = prefix + "_",
      callback = function() {
        dispatcher.bind("nodes", "click", function () { console.log("Not Impl");} );
        dispatcher.bind("edges", "click", function () { console.log("Not Impl");} );
      };
    createButton("edit", callback);
  };
  
  this.addControlExpand = function() {
    var prefix = "control_expand",
      idprefix = prefix + "_",
      callback = function() {
        console.log("Not implemented");
      };
    createButton("expand", callback);
  };
  
  this.addControlDelete = function() {
    var prefix = "control_delete",
      idprefix = prefix + "_",
      callback = function() {
        console.log("Not implemented");
      };
    createButton("delete", callback);
  };
  
  this.addControlConnect = function() {
    var prefix = "control_connect",
      idprefix = prefix + "_",
      callback = function() {
        console.log("Not implemented");
      };
    createButton("connect", callback);
  };
  
  
  
  this.addAll = function () {
    self.addControlDrag();
    self.addControlEdit();
    self.addControlExpand();
    self.addControlDelete();
    self.addControlConnect();
  };
  
}