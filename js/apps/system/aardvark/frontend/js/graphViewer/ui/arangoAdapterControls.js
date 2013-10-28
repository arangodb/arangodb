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
  
  this.addControlChangeCollections = function(callback) {
    var prefix = "control_adapter_collections",
      idprefix = prefix + "_";
      
    adapter.getCollections(function(nodeCols, edgeCols) {
      adapter.getGraphs(function(graphs) {
        uiComponentsHelper.createButton(baseClass, list, "Collections", prefix, function() {
          modalDialogHelper.createModalDialog("Switch Collections",
            idprefix, [{
              type: "decission",
              id: "collections",
              group: "loadtype",
              text: "Select existing collections",
              isDefault: (adapter.getGraphName() === undefined),
              interior: [
                {
                  type: "list",
                  id: "node_collection",
                  text: "Vertex collection",
                  objects: nodeCols,
                  selected: adapter.getNodeCollection()
                },{
                  type: "list",
                  id: "edge_collection",
                  text: "Edge collection",
                  objects: edgeCols,
                  selected: adapter.getEdgeCollection()
                }
              ]
            },{
              type: "decission",
              id: "graphs",
              group: "loadtype",
              text: "Select existing graph",
              isDefault: (adapter.getGraphName() !== undefined),
              interior: [
                {
                  type: "list",
                  id: "graph",
                  objects: graphs,
                  selected: adapter.getGraphName()
                }
              ]
            },{
              type: "checkbox",
              text: "Start with random vertex",
              id: "random",
              selected: true
            },{
              type: "checkbox",
              id: "undirected",
              selected: (adapter.getDirection() === "any")
            }], function () {
              var nodes = $("#" + idprefix + "node_collection")
                .children("option")
                .filter(":selected")
                .text(),
                edges = $("#" + idprefix + "edge_collection")
                  .children("option")
                  .filter(":selected")
                  .text(),
                graph = $("#" + idprefix + "graph")
                  .children("option")
                  .filter(":selected")
                  .text(),
                undirected = !!$("#" + idprefix + "undirected").attr("checked"),
                random = !!$("#" + idprefix + "random").attr("checked"),
                selected = $("input[type='radio'][name='loadtype']:checked").attr("id");
              if (selected === idprefix + "collections") {
                adapter.changeToCollections(nodes, edges, undirected);
              } else {
                adapter.changeToGraph(graph, undirected);
              }
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
    });
  };
  
  this.addControlChangePriority = function() {
    var prefix = "control_adapter_priority",
      idprefix = prefix + "_",
      prioList = adapter.getPrioList();

      uiComponentsHelper.createButton(baseClass, list, "Group By", prefix, function() {
        modalDialogHelper.createModalChangeDialog("Group By",
          idprefix, [{
            type: "extendable",
            id: "attribute",
            objects: prioList
          }], function () {
            var list = $("input[id^=" + idprefix + "attribute_]"),
              prios = [];
            list.each(function(i, t) {
              var val = $(t).attr("value");
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
    /*
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
            var  = $("#" + idprefix + "nodecollection").attr("value"),
              edges = $("#" + idprefix + "edgecollection").attr("value"),
              undirected = !!$("#" + idprefix + "undirected").attr("checked");
            adapter.changeTo(nodes, edges, undirected);

          }
        );
      });
    });
    */
  };
  
  this.addAll = function() {
    this.addControlChangeCollections();
    this.addControlChangePriority();
  };
}
