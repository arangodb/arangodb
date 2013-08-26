/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document*/
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
function LayouterControls(list, layouter) {
  "use strict";
  
  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (layouter === undefined) {
    throw "The Layouter has to be given.";
  }
  var self = this,
    baseClass = "layout";
  
  this.addControlGravity = function() {
    var prefix = "control_layout_gravity",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Gravity", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Gravity Strength",
        idprefix, [{
          type: "text",
          id: "value"
        }], function () {
          var value = $("#" + idprefix + "value").attr("value");
          layouter.changeTo({
            gravity: value
          });
        }
      );
    });
  };
  
  this.addControlCharge = function() {
    var prefix = "control_layout_charge",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Charge", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Charge Strength",
        idprefix, [{
          type: "text",
          id: "value"
        }], function () {
          var value = $("#" + idprefix + "value").attr("value");
          layouter.changeTo({
            charge: value
          });
        }
      );
    });
  };
  
  this.addControlDistance = function() {
    var prefix = "control_layout_distance",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(baseClass, list, "Distance", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Distance Strength",
        idprefix, [{
          type: "text",
          id: "value"
        }], function () {
          var value = $("#" + idprefix + "value").attr("value");
          layouter.changeTo({
            distance: value
          });
        }
      );
    });
  };
  
  
  this.addAll = function () {
    self.addControlDistance();
    self.addControlGravity();
    self.addControlCharge();
  };
  
}