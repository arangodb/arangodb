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
function NodeShaperControls(list, shaper) {
  "use strict";
  
  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (shaper === undefined) {
    throw "The NodeShaper has to be given.";
  }
  var self = this,
  
  createModalDialog = function(title, idprefix, objects, callback) {
    var table =  modalDialogHelper.modalDivTemplate(title, idprefix, callback);
    
    _.each(objects, function(o) {
      var tr = document.createElement("tr"),
      labelTh = document.createElement("th"),
      contentTh = document.createElement("th"),
      input;
      
      table.appendChild(tr);
      tr.appendChild(labelTh);
      labelTh.className = "collectionTh capitalize";
      labelTh.appendChild(document.createTextNode(o.id + ":"));
      tr.appendChild(contentTh);
      contentTh.className = "collectionTh";
      switch(o.type) {
        case "text":
          input = document.createElement("input");
          input.type = "text";
          input.id = idprefix + o.id;
          contentTh.appendChild(input);
          break;
        default:
          //Sorry unknown
          table.removeChild(tr);
          break;
      }
    });
    $("#" + idprefix + "modal").modal('show');
  };
  
  this.addControlOpticShapeCircle = function() {
    var prefix = "control_circle",
    idprefix = prefix + "_",
    callback = function() {
      createModalDialog("Switch to Circle",
        idprefix, [{
          type: "text",
          id: "radius"
        }], function () {
          var r = $("#" + idprefix + "radius").attr("value");
          shaper.changeTo({
            shape: {
              type: NodeShaper.shapes.CIRCLE,
              radius: r
            }
          });
        }
      );
    },
    button = document.createElement("li");
    button.className = "graph_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode("Circle"));
    list.appendChild(button);
    button.onclick = callback;
  };
  
  this.addControlOpticShapeRect = function() {
    var prefix = "control_rect",
    idprefix = prefix + "_",
    callback = function() {
      createModalDialog("Switch to Rectangle",
        idprefix, [{
          type: "text",
          id: "width"
        },{
          type: "text",
          id: "height"
        }], function () {
          var w = $("#" + idprefix + "width").attr("value"),
          h = $("#" + idprefix + "height").attr("value");
          shaper.changeTo({
            shape: {
              type: NodeShaper.shapes.RECT,
              width: w,
              height: h
            }
          });
        }
      );
    },
    button = document.createElement("li");
    button.className = "graph_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode("Rectangle"));
    list.appendChild(button);
    button.onclick = callback;
  };
  
  this.addControlOpticLabel = function() {
    var prefix = "control_label",
    idprefix = prefix + "_",
    callback = function() {
      createModalDialog("Switch Label Attribute",
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
    self.addControlOpticShapeCircle();
    self.addControlOpticShapeRect();
    self.addControlOpticLabel();
  };
  
  this.addAllActions = function () {
  
  };
  
  this.addAll = function () {
    self.addAllOptics();
    self.addAllActions();
  };
  
}