/*global $, _, d3*/
/*global document, Storage, localStorage, window*/
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
    colourDiv,

    fillColourDiv = function(mapping) {
      while (colourDiv.hasChildNodes()) {
          colourDiv.removeChild(colourDiv.lastChild);
      }
      var list = document.createElement("ul");
      colourDiv.appendChild(list);
      _.each(mapping, function(obj, col) {
        var ul = document.createElement("ul"),
          els = obj.list,
          fore = obj.front;
        ul.style.backgroundColor = col;
        ul.style.color = fore;
        _.each(els, function(e) {
          var li = document.createElement("li");
          li.appendChild(document.createTextNode(e));
          ul.appendChild(li);
        });
        list.appendChild(ul);
      });
    };

  this.addControlOpticShapeNone = function() {
    uiComponentsHelper.createButton(list, "None", "control_node_none", function() {
      shaper.changeTo({
        shape: {
          type: NodeShaper.shapes.NONE
        }
      });
    });
  };

  this.applyLocalStorage = function(obj) {
    if (Storage !== "undefined") {
      try {
        var toStore = JSON.parse(localStorage.getItem('graphSettings')),
        graphName = (window.location.hash).split("/")[1],
        dbName = (window.location.pathname).split('/')[2],
        combinedGraphName = graphName + dbName;

        _.each(obj, function(value, key) {
          if (key !== undefined) {
            toStore[combinedGraphName].viewer.nodeShaper[key] = value;
          }
        });

        localStorage.setItem('graphSettings', JSON.stringify(toStore));
      }
      catch (e) {
        console.log(e);
      }
    }
  };

  this.addControlOpticShapeCircle = function() {
    var prefix = "control_node_circle",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Circle", prefix, function() {
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
    uiComponentsHelper.createButton(list, "Rectangle", prefix, function() {
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
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
      modalDialogHelper.createModalChangeDialog("Change label attribute",
        idprefix, [{
          type: "text",
          id: "key"
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          var shaperObj = {
            label: key
          };
          self.applyLocalStorage(shaperObj);
          shaper.changeTo(shaperObj);
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
    uiComponentsHelper.createButton(list, "Single Colour", prefix, function() {
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
    uiComponentsHelper.createButton(list, "Colour by Attribute", prefix, function() {
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
    uiComponentsHelper.createButton(list, "Expansion Colour", prefix, function() {
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

  this.addControlOpticLabelAndColour = function(adapter) {
    var prefix = "control_node_labelandcolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
      modalDialogHelper.createModalChangeDialog("Change label attribute",
        idprefix, [{
          type: "text",
          id: "label-attribute",
          text: "Vertex label attribute",
          value: shaper.getLabel() || ""
        },{
          type: "decission",
          id: "samecolour",
          group: "colour",
          text: "Use this attribute for coloring, too",
          isDefault: (shaper.getLabel() === shaper.getColor())
        },{
          type: "decission",
          id: "othercolour",
          group: "colour",
          text: "Use different attribute for coloring",
          isDefault: (shaper.getLabel() !== shaper.getColor()),
          interior: [
            {
              type: "text",
              id: "colour-attribute",
              text: "Color attribute",
              value: shaper.getColor() || ""
            }
          ]
        }], function () {
          var key = $("#" + idprefix + "label-attribute").attr("value"),
            colourkey = $("#" + idprefix + "colour-attribute").attr("value"),
            selected = $("input[type='radio'][name='colour']:checked").attr("id");
          if (selected === idprefix + "samecolour") {
            colourkey = key;
          }
          var shaperObj = {
            label: key,
            color: {
              type: "attribute",
              key: colourkey
            }
          };
          self.applyLocalStorage(shaperObj);
          shaper.changeTo(shaperObj);
          if (colourDiv === undefined) {
            colourDiv = self.createColourMappingList();
          }
        }
      );
    });
  };

  this.addControlOpticLabelAndColourList = function(adapter) {
    var prefix = "control_node_labelandcolourlist",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
      modalDialogHelper.createModalChangeDialog("Change label attribute",
        idprefix, [{
          type: "extendable",
          id: "label",
          text: "Vertex label attribute",
          objects: shaper.getLabel()
        },{
          type: "decission",
          id: "samecolour",
          group: "colour",
          text: "Use this attribute for coloring, too",
          isDefault: (shaper.getLabel() === shaper.getColor())
        },{
          type: "decission",
          id: "othercolour",
          group: "colour",
          text: "Use different attribute for coloring",
          isDefault: (shaper.getLabel() !== shaper.getColor()),
          interior: [
            {
              type: "extendable",
              id: "colour",
              text: "Color attribute",
              objects: shaper.getColor() || ""
            }
          ]
        }], function () {
          var lblList = $("input[id^=" + idprefix + "label_]"),
            colList = $("input[id^=" + idprefix + "colour_]"),
            selected = $("input[type='radio'][name='colour']:checked").attr("id"),
            labels = [], colours = [];
          lblList.each(function(i, t) {
            var val = $(t).val();
            if (val !== "") {
              labels.push(val);
            }
          });
          colList.each(function(i, t) {
            var val = $(t).val();
            if (val !== "") {
              colours.push(val);
            }
          });
          if (selected === idprefix + "samecolour") {
            colours = labels;
          }

          var shaperObj = {
            label: labels,
            color: {
              type: "attribute",
              key: colours
            }
          };

          self.applyLocalStorage(shaperObj);

          shaper.changeTo(shaperObj);
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
