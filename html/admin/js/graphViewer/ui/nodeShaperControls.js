/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document*/
/*global NodeShaper*/
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
function NodeShaperControls(list, shaper) {
  "use strict";
  
  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (shaper === undefined) {
    throw "The NodeShaper has to be given.";
  }
  var self = this,
  modalDivTemplate = function (title, idprefix, callback) {
    // Create needed Elements
    var div = document.createElement("div"),
    headerDiv = document.createElement("div"),
    buttonDismiss = document.createElement("button"),
    header = document.createElement("h3"),
    footerDiv = document.createElement("div"),
    buttonCancel = document.createElement("button"),
    buttonSubmit = document.createElement("button"),
    bodyDiv = document.createElement("div"),
    bodyTable = document.createElement("table");
        
    // Set Classnames and attributes.
    div.id = idprefix + "modal";
    div.className = "modal hide fade";
    div.setAttribute("tabindex", "-1");
    div.setAttribute("role", "dialog");
    div.setAttribute("aria-labelledby", "myModalLabel");
    div.setAttribute("aria-hidden", true);
    div.style.display = "none";
    div.onhidden = function() {
      document.body.removeChild(div);
    };
    
    
    headerDiv.className = "modal_header";
    
    buttonDismiss.className = "close";
    buttonDismiss.dataDismiss = "modal";
    buttonDismiss.ariaHidden = "true";
    buttonDismiss.appendChild(document.createTextNode("x"));
    
    header.appendChild(document.createTextNode(title));
    
    bodyDiv.className = "modal_body";
    
    footerDiv.className = "modal_footer";
    
    buttonCancel.id = idprefix + "cancel";
    buttonCancel.className = "btn btn-danger pull-left";
    buttonCancel.appendChild(document.createTextNode("Cancel"));
    
    buttonSubmit.id = idprefix + "submit";
    buttonSubmit.className = "btn btn-success pull-right";
    buttonSubmit.appendChild(document.createTextNode("Switch"));
    
    // Append in correct ordering
    div.appendChild(headerDiv);
    div.appendChild(bodyDiv);
    div.appendChild(footerDiv);
    
    headerDiv.appendChild(buttonDismiss);
    headerDiv.appendChild(header);
    
    bodyDiv.appendChild(bodyTable);
    
    footerDiv.appendChild(buttonCancel);
    footerDiv.appendChild(buttonSubmit);
    document.body.appendChild(div);
    
    // Add click events
    buttonDismiss.onclick = function() {
      $("#" + idprefix + "modal").modal('hide');
    };
    buttonCancel.onclick = function() {
      $("#" + idprefix + "modal").modal('hide');
    };
    buttonSubmit.onclick = function() {
      callback();
      $("#" + idprefix + "modal").modal('hide');
    };
    
    // Return the table which has to be filled somewhere else
    return bodyTable;
  },
  
  createModalDialog = function(title, idprefix, objects, callback) {
    var table =  modalDivTemplate(title, idprefix, callback);
    
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