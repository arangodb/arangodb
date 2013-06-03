/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document*/
/*global modalDialogHelper, uiComponentsHelper*/
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
function ArangoAdapterControls(list, adapter) {
  "use strict";
  
  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (adapter === undefined) {
    throw "The ArangoAdapter has to be given.";
  }
  var self = this,
    baseClass = "adapter";
  
  this.addControlChangeCollections = function() {
    var prefix = "control_adapter_collections",
      idprefix = prefix + "_";
    adapter.getCollections(function(nodeCols, edgeCols) {
      uiComponentsHelper.createButton(baseClass, list, "Collections", prefix, function() {
        modalDialogHelper.createModalDialog("Switch Collections",
          idprefix, [{
            type: "list",
            id: "nodecollection",
            objects: nodeCols
          },{
            type: "list",
            id: "edgecollection",
            objects: edgeCols
          },{
            type: "checkbox",
            id: "undirected"
          }], function () {
            var nodes = $("#" + idprefix + "nodecollection").attr("value"),
              edges = $("#" + idprefix + "edgecollection").attr("value"),
              undirected = !!$("#" + idprefix + "undirected").attr("checked");
            adapter.changeTo(nodes, edges, undirected);
          }
        );
      });
    });
    
  };
  
  this.addAll = function() {
    this.addControlChangeCollections();
  };
}