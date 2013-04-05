/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document*/
/*global NodeShaper, modalDialogHelper*/
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
  var self = this;
  
  this.addControlOpticShapeNone = function() {
    var prefix = "control_none",
    idprefix = prefix + "_",
    callback = function() {
      shaper.changeTo({
        shape: {
          type: NodeShaper.shapes.NONE
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
  
  this.addControlOpticShapeCircle = function() {
    var prefix = "control_circle",
    idprefix = prefix + "_",
    callback = function() {
      modalDialogHelper.createModalDialog("Switch to Circle",
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
      modalDialogHelper.createModalDialog("Switch to Rectangle",
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
  
  //////////////////////////////////////////////////////////////////
  //  Colour Buttons
  //////////////////////////////////////////////////////////////////
  
  this.addControlOpticSingleColour = function() {
    var prefix = "control_singlecolour",
    idprefix = prefix + "_",
    callback = function() {
      modalDialogHelper.createModalDialog("Switch to Colour",
        idprefix, [{
          type: "text",
          id: "fill"
        },{
          type: "text",
          id: "stroke"
        }], function () {
          var fill = $("#" + idprefix + "fill").attr("value"),
          stroke = $("#" + idprefix + "stroke").attr("value");
          shaper.changeTo({
            color: {
              type: "single",
              fill: fill,
              stroke: stroke
            }
          });
        }
      );
    },
    button = document.createElement("li");
    button.className = "graph_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode("Single Colour"));
    list.appendChild(button);
    button.onclick = callback;
  };
  
  this.addControlOpticAttributeColour = function() {
    var prefix = "control_attributecolour",
    idprefix = prefix + "_",
    callback = function() {
      modalDialogHelper.createModalDialog("Display colour by attribute",
        idprefix, [{
          type: "text",
          id: "key"
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          shaper.changeTo({
            color: {
              type: "attribute",
              key: key
            }
          });
        }
      );
    },
    button = document.createElement("li");
    button.className = "graph_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode("Colour by Attribute"));
    list.appendChild(button);
    button.onclick = callback;
  };
  
  this.addControlOpticExpandColour = function() {
    var prefix = "control_expandcolour",
    idprefix = prefix + "_",
    callback = function() {
      modalDialogHelper.createModalDialog("Display colours for expansion",
        idprefix, [{
          type: "text",
          id: "expanded"
        },{
          type: "text",
          id: "collapsed"
        }], function () {
          var expanded = $("#" + idprefix + "expanded").attr("value"),
          collapsed = $("#" + idprefix + "collapsed").attr("value");
          shaper.changeTo({
            color: {
              type: "expand",
              expanded: expanded,
              collapsed: collapsed
            }
          });
        }
      );
    },
    button = document.createElement("li");
    button.className = "graph_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode("Expansion Colour"));
    list.appendChild(button);
    button.onclick = callback;
  };
  
  this.addAllOptics = function () {
    self.addControlOpticShapeNone();
    self.addControlOpticShapeCircle();
    self.addControlOpticShapeRect();
    self.addControlOpticLabel();
    self.addControlOpticSingleColour();
    self.addControlOpticAttributeColour();
    self.addControlOpticExpandColour();
  };
  
  this.addAllActions = function () {
  
  };
  
  this.addAll = function () {
    self.addAllOptics();
    self.addAllActions();
  };
  
}