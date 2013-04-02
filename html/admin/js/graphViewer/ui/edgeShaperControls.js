/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document*/
/*global EdgeShaper, modalDialogHelper*/
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
function EdgeShaperControls(list, shaper) {
  "use strict";
  
  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (shaper === undefined) {
    throw "The EdgeShaper has to be given.";
  }
  var self = this;
  
  this.addControlOpticShapeNone = function() {
    var prefix = "control_none",
    idprefix = prefix + "_",
    callback = function() {
      shaper.changeTo({
        shape: {
          type: EdgeShaper.shapes.NONE
        }
      });
    },
    button = document.createElement("li");
    button.className = "graph_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode("None"));
    list.appendChild(button);
    button.onclick = callback;
  };
  
  this.addControlOpticShapeArrow = function() {
    var prefix = "control_arrow",
    idprefix = prefix + "_",
    callback = function() {
      shaper.changeTo({
        shape: {
          type: EdgeShaper.shapes.ARROW
        }
      });
    },
    button = document.createElement("li");
    button.className = "graph_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode("Arrow"));
    list.appendChild(button);
    button.onclick = callback;
  };
  

    
  this.addControlOpticLabel = function() {
    var prefix = "control_label",
    idprefix = prefix + "_",
    callback = function() {
      modalDialogHelper.createModalDialog("Switch Label Attribute",
        idprefix, [{
          type: "text",
          id: "key"
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          shaper.changeTo({
            label: key
          });
        }
      );
    },
    button = document.createElement("li");
    button.className = "graph_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode("Label"));
    list.appendChild(button);
    button.onclick = callback;
  };
  
  this.addAllOptics = function () {
    self.addControlOpticShapeNone();
    self.addControlOpticShapeArrow();
    self.addControlOpticLabel();
  };
  
  this.addAllActions = function () {
  
  };
  
  this.addAll = function () {
    self.addAllOptics();
    self.addAllActions();
  };
  
}