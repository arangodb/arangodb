/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document*/
/*global NodeShaper, modalDialogHelper, uiComponentsHelper*/
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
    baseClass = "graph",
    colourDiv,
    
    fillColourDiv = function(mapping) {
      while (colourDiv.hasChildNodes()) {
          colourDiv.removeChild(colourDiv.lastChild);
      }
      var list = document.createElement("ul");
      colourDiv.appendChild(list);
      _.each(mapping, function(els, col) {
        var li = document.createElement("li");
        li.style.backgroundColor = col;
        li.appendChild(document.createTextNode(els.join(", ")));
        list.appendChild(li);
      });
    };
  
  this.addControlOpticShapeNone = function() {
    uiComponentsHelper.createButton(baseClass, list, "None", "control_node_none", function() {
      shaper.changeTo({
        shape: {
          type: NodeShaper.shapes.NONE
        }
      });
    });
  };
  
  this.addControlOpticShapeCircle = function() {
    var prefix = "control_node_circle",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Circle", prefix, function() {
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
    });
  };
  
  this.addControlOpticShapeRect = function() {
    var prefix = "control_node_rect",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Rectangle", prefix, function() {
      modalDialogHelper.createModalDialog("Switch to Rectangle",
        "control_node_rect_", [{
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
    });
  };
  
  this.addControlOpticLabel = function() {
    var prefix = "control_node_label",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Label", prefix, function() {
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
    });
  };
  
  //////////////////////////////////////////////////////////////////
  //  Colour Buttons
  //////////////////////////////////////////////////////////////////
  
  this.addControlOpticSingleColour = function() {
    var prefix = "control_node_singlecolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Single Colour", prefix, function() {
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
    });
  };
  
  this.addControlOpticAttributeColour = function() {
    var prefix = "control_node_attributecolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Colour by Attribute", prefix, function() {
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
    });
  };
  
  this.addControlOpticExpandColour = function() {
    var prefix = "control_node_expandcolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Expansion Colour", prefix, function() {
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
    });
  };
  
  //////////////////////////////////////////////////////////////////
  //  Mixed Buttons
  //////////////////////////////////////////////////////////////////  
  
  this.addControlOpticLabelAndColour = function() {
    var prefix = "control_node_labelandcolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Label", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Label Attribute",
        idprefix, [{
          type: "text",
          id: "key"
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          shaper.changeTo({
            label: key,
            color: {
              type: "attribute",
              key: key
            }
          });
          if (colourDiv === undefined) {
            colourDiv = self.createColourMappingList();
          }
          
        }
      );
    });
  };
  
  //////////////////////////////////////////////////////////////////
  //  Multiple Buttons
  ////////////////////////////////////////////////////////////////// 
  
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
  
  //////////////////////////////////////////////////////////////////
  //  Colour Mapping List
  //////////////////////////////////////////////////////////////////  
  
  this.createColourMappingList = function() {
    if (colourDiv !== undefined) {
      return colourDiv;
    }
    colourDiv = document.createElement("div");
    colourDiv.id = "node_colour_list";
    fillColourDiv(shaper.getColourMapping());
    shaper.setColourMappingListener(fillColourDiv);
    return colourDiv;
  };
}