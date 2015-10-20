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
function GharialAdapterControls(list, adapter) {
  "use strict";

  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (adapter === undefined) {
    throw "The GharialAdapter has to be given.";
  }
  this.addControlChangeGraph = function(callback) {
    var prefix = "control_adapter_graph",
      idprefix = prefix + "_";

    adapter.getGraphs(function(graphs) {
      uiComponentsHelper.createButton(list, "Switch Graph", prefix, function() {
        modalDialogHelper.createModalDialog("Switch Graph",
          idprefix, [{
            type: "list",
            id: "graph",
            objects: graphs,
            text: "Select graph",
            selected: adapter.getGraphName()
          },{
            type: "checkbox",
            text: "Start with random vertex",
            id: "random",
            selected: true
          },//currently disabled outbound only view
          /*{
            type: "checkbox",
            id: "undirected",
            selected: (adapter.getDirection() === "any")
          }*/], function () {
            var graph = $("#" + idprefix + "graph")
                .children("option")
                .filter(":selected")
                .text(),
              undirected = !!$("#" + idprefix + "undirected").prop("checked"),
              random = !!$("#" + idprefix + "random").prop("checked");
            adapter.changeToGraph(graph, undirected);
            if (random) {
              adapter.loadRandomNode(callback);
              return;
            }
            if (_.isFunction(callback)) {
              callback();
            }
          }
        );
      });
    });
  };

  this.addControlChangePriority = function() {
    var prefix = "control_adapter_priority",
      idprefix = prefix + "_",
      label = "Group vertices";

    uiComponentsHelper.createButton(list, label, prefix, function() {
      modalDialogHelper.createModalChangeDialog(label + " by attribute",
        idprefix, [{
          type: "extendable",
          id: "attribute",
          objects: adapter.getPrioList()
        }], function () {
          var attrList = $("input[id^=" + idprefix + "attribute_]"),
            prios = [];
          _.each(attrList, function(t) {
            var val = $(t).val();
            if (val !== "") {
              prios.push(val);
            }
          });
          adapter.changeTo({
            prioList: prios
          });
        }
      );
    });
  };

  this.addAll = function() {
    this.addControlChangeGraph();
    this.addControlChangePriority();
  };
}
