/*global $, _, d3*/
/*global document, Storage, localStorage, window*/
/*global EdgeShaper, modalDialogHelper, uiComponentsHelper*/
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
    var prefix = "control_edge_none",
    idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "None", prefix, function() {
      shaper.changeTo({
        shape: {
          type: EdgeShaper.shapes.NONE
        }
      });
    });
  };

  this.addControlOpticShapeArrow = function() {
    var prefix = "control_edge_arrow",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Arrow", prefix, function() {
      shaper.changeTo({
        shape: {
          type: EdgeShaper.shapes.ARROW
        }
      });
    });
  };



  this.addControlOpticLabel = function() {
    var prefix = "control_edge_label",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Label Attribute",
        idprefix, [{
          type: "text",
          id: "key",
          text: "Edge label attribute",
          value: shaper.getLabel()
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          shaper.changeTo({
            label: key
          });
        }
      );
    });
  };

  this.addControlOpticLabelList = function() {
    var prefix = "control_edge_label",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
      modalDialogHelper.createModalDialog("Change Label Attribute",
        idprefix, [{
          type: "extendable",
          id: "label",
          text: "Edge label attribute",
          objects: shaper.getLabel()
        }], function () {
          var lblList = $("input[id^=" + idprefix + "label_]"),
            labels = [];
          lblList.each(function(i, t) {
            var val = $(t).val();
            if (val !== "") {
              labels.push(val);
            }
          });

          var obj = {
            label: labels
          };
          self.applyLocalStorage(obj);
          shaper.changeTo(obj);
        }
      );
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
            if (!toStore[combinedGraphName].viewer.hasOwnProperty('edgeShaper')) {
              toStore[combinedGraphName].viewer.edgeShaper = {};
            } 
            toStore[combinedGraphName].viewer.edgeShaper[key] = value;
          }
        });

        localStorage.setItem('graphSettings', JSON.stringify(toStore));
      }
      catch (e) {
        console.log(e);
      }
    }
  };

  this.addControlOpticSingleColour = function() {
    var prefix = "control_edge_singlecolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Single Colour", prefix, function() {
      modalDialogHelper.createModalDialog("Switch to Colour",
        idprefix, [{
          type: "text",
          id: "stroke"
        }], function () {
          var stroke = $("#" + idprefix + "stroke").attr("value");
          shaper.changeTo({
            color: {
              type: "single",
              stroke: stroke
            }
          });
        }
      );
    });
  };

  this.addControlOpticAttributeColour = function() {
    var prefix = "control_edge_attributecolour",
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

  this.addControlOpticGradientColour = function() {
    var prefix = "control_edge_gradientcolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Gradient Colour", prefix, function() {
      modalDialogHelper.createModalDialog("Change colours for gradient",
        idprefix, [{
          type: "text",
          id: "source"
        },{
          type: "text",
          id: "target"
        }], function () {
          var source = $("#" + idprefix + "source").attr("value"),
          target = $("#" + idprefix + "target").attr("value");
          shaper.changeTo({
            color: {
              type: "gradient",
              source: source,
              target: target
            }
          });
        }
      );
    });
  };

  this.addAllOptics = function () {
    self.addControlOpticShapeNone();
    self.addControlOpticShapeArrow();
    self.addControlOpticLabel();
    self.addControlOpticSingleColour();
    self.addControlOpticAttributeColour();
    self.addControlOpticGradientColour();
  };

  this.addAllActions = function () {

  };

  this.addAll = function () {
    self.addAllOptics();
    self.addAllActions();
  };

}
