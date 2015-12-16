/*global $, _, d3*/
/*global document, window, prompt*/
/*global modalDialogHelper, uiComponentsHelper */
/*global EventDispatcher, arangoHelper*/
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

function EventDispatcherControls(list, nodeShaper, edgeShaper, start, dispatcherConfig) {
  "use strict";

  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (nodeShaper === undefined) {
    throw "The NodeShaper has to be given.";
  }
  if (edgeShaper === undefined) {
    throw "The EdgeShaper has to be given.";
  }
  if (start === undefined) {
    throw "The Start callback has to be given.";
  }

  var self = this,
    /*
    icons = {
      expand: "expand",
      add: "add",
      trash: "trash",
      drag: "drag",
      edge: "connect",
      edit: "edit",
      view: "view"
    },
    */
    icons = {
      expand: {
        icon: "hand-pointer-o",
        title: "Expand a node."
      },
      add: {
        icon: "plus-square",
        title: "Add a node."
      },
      trash: {
        icon: "minus-square",
        title: "Remove a node/edge."
      },
      drag: {
        icon: "hand-rock-o",
        title: "Drag a node."
      },
      edge: {
        icon: "external-link-square",
        title: "Create an edge between two nodes."
      },
      edit: {
        icon: "pencil-square",
        title: "Edit attributes of a node."
      },
      view: {
        icon: "search",
        title: "View attributes of a node."
      }
    },
    dispatcher = new EventDispatcher(nodeShaper, edgeShaper, dispatcherConfig),
    adapter = dispatcherConfig.edgeEditor.adapter,
    askForCollection = (!!adapter
      && _.isFunction(adapter.useNodeCollection)
      && _.isFunction(adapter.useEdgeCollection)),


    appendToList = function(button) {
      list.appendChild(button);
    },
    createIcon = function(icon, title, callback) {
      var btn = uiComponentsHelper.createIconButton(
        icon,
        "control_event_" + title,
        callback
      );
      appendToList(btn);
    },
    rebindNodes = function(actions) {
      dispatcher.rebind("nodes", actions);
    },
    rebindEdges = function(actions) {
      dispatcher.rebind("edges", actions);
    },
    rebindSVG = function(actions) {
      dispatcher.rebind("svg", actions);
    },
    getCursorPosition = function (ev) {
      var e = ev || window.event,
        res = {};
      res.x = e.clientX;
      res.y = e.clientY;
      res.x += document.body.scrollLeft;
      res.y += document.body.scrollTop;
      return res;
    },
    getCursorPositionInSVG = function (ev) {
      var pos = getCursorPosition(ev),
          off = $('svg#graphViewerSVG').offset(),
          svg, bBox, bCR;
      svg = d3.select("svg#graphViewerSVG").node();
      // Normal case. SVG has no clipped view box.
      bCR = svg.getBoundingClientRect();
      if ($("svg#graphViewerSVG").height() <= bCR.height ) {
        return {
          x: pos.x - off.left,
          y: pos.y - off.top
        };
      }
      // Firefox case. SVG has a clipped view box.
      bBox = svg.getBBox();
      return {
        x: pos.x - (bCR.left - bBox.x),
        y: pos.y - (bCR.top - bBox.y)
      };
    },
    callbacks = {
      nodes: {},
      edges: {},
      svg: {}
    },

  /*******************************************
  * Create callbacks wenn clicking on objects
  *
  *******************************************/

    createNewNodeCB = function() {
      var prefix = "control_event_new_node",
        idprefix = prefix + "_",
        createCallback = function(ev) {
          var pos = getCursorPositionInSVG(ev);
          modalDialogHelper.createModalCreateDialog(
            "Create New Node",
            idprefix,
            {},
            function(data) {
              dispatcher.events.CREATENODE(data, function(node) {
                $("#" + idprefix + "modal").modal('hide');
                nodeShaper.reshapeNodes();
                start();
              }, pos.x, pos.y)();
            }
          );
        };
      callbacks.nodes.newNode = createCallback;
    },
    createViewCBs = function() {
      var prefix = "control_event_view",
        idprefix = prefix + "_",
        nodeCallback = function(n) {
          modalDialogHelper.createModalViewDialog(
            "View Node " + n._id,
            "control_event_node_view_",
            n._data,
            function() {
              modalDialogHelper.createModalEditDialog(
                "Edit Node " + n._id,
                "control_event_node_edit_",
                n._data,
                function(newData) {
                  dispatcher.events.PATCHNODE(n, newData, function() {
                    $("#control_event_node_edit_modal").modal('hide');
                  })();
                }
              );
            }
          );
        },
        edgeCallback = function(e) {
          modalDialogHelper.createModalViewDialog(
            "View Edge " + e._id,
            "control_event_edge_view_",
            e._data,
            function() {
              modalDialogHelper.createModalEditDialog(
                "Edit Edge " + e._id,
                "control_event_edge_edit_",
                e._data,
                function(newData) {
                  dispatcher.events.PATCHEDGE(e, newData, function() {
                    $("#control_event_edge_edit_modal").modal('hide');
                  })();
                }
              );
            }
          );
        };

      callbacks.nodes.view = nodeCallback;
      callbacks.edges.view = edgeCallback;
    },
    createConnectCBs = function() {
      var prefix = "control_event_connect",
        idprefix = prefix + "_",
        nodesDown = dispatcher.events.STARTCREATEEDGE(function(startNode, ev) {
          var pos = getCursorPositionInSVG(ev),
             moveCB = edgeShaper.addAnEdgeFollowingTheCursor(pos.x, pos.y);
          dispatcher.bind("svg", "mousemove", function(ev) {
            var pos = getCursorPositionInSVG(ev);
            moveCB(pos.x, pos.y);
          });
        }),
        nodesUp = dispatcher.events.FINISHCREATEEDGE(function(edge){
          edgeShaper.removeCursorFollowingEdge();
          dispatcher.bind("svg", "mousemove", function(){
            return undefined;
          });
          start();
        }),
        svgUp = function() {
          dispatcher.events.CANCELCREATEEDGE();
          edgeShaper.removeCursorFollowingEdge();
          dispatcher.bind("svg", "mousemove", function(){
            return undefined;
          });
        };
      callbacks.nodes.startEdge = nodesDown;
      callbacks.nodes.endEdge = nodesUp;
      callbacks.svg.cancelEdge = svgUp;
    },

    createEditsCBs = function() {
      var nodeCallback = function(n) {
        /*var deleteCallback = function() {
          console.log("callback");
          dispatcher.events.DELETENODE(function() {
            $("#control_event_node_delete_modal").modal('hide');
            nodeShaper.reshapeNodes();
            edgeShaper.reshapeEdges();
            start();
          })(n);
        };*/

        arangoHelper.openDocEditor(n._id, 'document');
          /*
          modalDialogHelper.createModalEditDialog(
            "Edit Node " + n._id,
            "control_event_node_edit_",
            n._data,
            function(newData) {
              dispatcher.events.PATCHNODE(n, newData, function() {
                $("#control_event_node_edit_modal").modal('hide');
              })();
            }
          );
          */
        },
        edgeCallback = function(e) {
          arangoHelper.openDocEditor(e._id, 'edge');
          /*modalDialogHelper.createModalEditDialog(
            "Edit Edge " + e._id,
            "control_event_edge_edit_",
            e._data,
            function(newData) {
              dispatcher.events.PATCHEDGE(e, newData, function() {
                $("#control_event_edge_edit_modal").modal('hide');
              })();
            }
          );*/
        };
      callbacks.nodes.edit = nodeCallback;
      callbacks.edges.edit = edgeCallback;
    },

    createDeleteCBs = function() {
      var nodeCallback = function(n) {
          modalDialogHelper.createModalDeleteDialog(
            "Delete Node " + n._id,
            "control_event_node_delete_",
            n,
            function(n) {
              dispatcher.events.DELETENODE(function() {
                $("#control_event_node_delete_modal").modal('hide');
                nodeShaper.reshapeNodes();
                edgeShaper.reshapeEdges();
                start();
              })(n);
            }
          );
        },
        edgeCallback = function(e) {
          modalDialogHelper.createModalDeleteDialog(
            "Delete Edge " + e._id,
            "control_event_edge_delete_",
            e,
            function(e) {
              dispatcher.events.DELETEEDGE(function() {
                $("#control_event_edge_delete_modal").modal('hide');
                nodeShaper.reshapeNodes();
                edgeShaper.reshapeEdges();
                start();
              })(e);
            }
          );
        };
      callbacks.nodes.del = nodeCallback;
      callbacks.edges.del = edgeCallback;
    },

    createSpotCB = function() {
     callbacks.nodes.spot = dispatcher.events.EXPAND;
    };

  createNewNodeCB();
  createViewCBs();
  createConnectCBs();
  createEditsCBs();
  createDeleteCBs();
  createSpotCB();

  /*******************************************
  * Raw rebind objects
  *
  *******************************************/
  this.dragRebinds = function() {
    return {
      nodes: {
        drag: dispatcher.events.DRAG
      }
    };
  };

  this.newNodeRebinds = function() {
    return {
      svg: {
        click: callbacks.nodes.newNode
      }
    };
  };

  this.viewRebinds = function() {
      return {
        nodes: {
          click: callbacks.nodes.view
        },
        edges: {
          click: callbacks.edges.view
        }
      };
  };

  this.connectNodesRebinds = function() {
    return {
      nodes: {
        mousedown: callbacks.nodes.startEdge,
        mouseup: callbacks.nodes.endEdge
      },
      svg: {
        mouseup: callbacks.svg.cancelEdge
      }
    };
  };

  this.editRebinds = function() {
      return {
        nodes: {
          click: callbacks.nodes.edit
        },
        edges: {
          click: callbacks.edges.edit
        }
      };
  };

  this.expandRebinds = function() {
    return {
      nodes: {
        click: callbacks.nodes.spot
      }
    };
  };

  this.deleteRebinds = function() {
    return {
      nodes: {
        click: callbacks.nodes.del
      },
      edges: {
        click: callbacks.edges.del
      }
    };
  };

  this.rebindAll = function(obj) {
    rebindNodes(obj.nodes);
    rebindEdges(obj.edges);
    rebindSVG(obj.svg);
  };

  /*******************************************
  * Inject controls into right-click menus
  *
  *******************************************/

  //nodeShaper.addMenuEntry("View", callbacks.nodes.view);
  nodeShaper.addMenuEntry("Edit", callbacks.nodes.edit);
  nodeShaper.addMenuEntry("Spot", callbacks.nodes.spot);
  nodeShaper.addMenuEntry("Trash", callbacks.nodes.del);

  //edgeShaper.addMenuEntry("View", callbacks.edges.view);
  edgeShaper.addMenuEntry("Edit", callbacks.edges.edit);
  edgeShaper.addMenuEntry("Trash", callbacks.edges.del);


  /*******************************************
  * Functions to add controls
  *
  *******************************************/


  this.addControlNewNode = function() {
    var icon = icons.add,
      idprefix = "select_node_collection",
      callback = function() {
        if (askForCollection && adapter.getNodeCollections().length > 1) {
          modalDialogHelper.createModalDialog("Select Vertex Collection",
            idprefix, [{
              type: "list",
              id: "vertex",
              objects: adapter.getNodeCollections(),
              text: "Select collection",
              selected: adapter.getSelectedNodeCollection()
            }], function () {
              var nodeCollection = $("#" + idprefix + "vertex")
                  .children("option")
                  .filter(":selected")
                  .text();
              adapter.useNodeCollection(nodeCollection);
            },
            "Select"
          );
        }
        self.rebindAll(self.newNodeRebinds());
      };
    createIcon(icon, "new_node", callback);
  };

  this.addControlView = function() {
    var icon = icons.view,
      callback = function() {
        self.rebindAll(self.viewRebinds());
      };
    createIcon(icon, "view", callback);
  };

  this.addControlDrag = function() {
    var icon = icons.drag,
      callback = function() {
        self.rebindAll(self.dragRebinds());
      };
    createIcon(icon, "drag", callback);
  };

  this.addControlEdit = function() {
    var icon = icons.edit,
      callback = function() {
        self.rebindAll(self.editRebinds());
      };
    createIcon(icon, "edit", callback);
  };

  this.addControlExpand = function() {
    var icon = icons.expand,
      callback = function() {
        self.rebindAll(self.expandRebinds());
      };
    createIcon(icon, "expand", callback);
  };

  this.addControlDelete = function() {
    var icon = icons.trash,
      callback = function() {
        self.rebindAll(self.deleteRebinds());
      };
    createIcon(icon, "delete", callback);
  };

  this.addControlConnect = function() {
    var icon = icons.edge,
      idprefix = "select_edge_collection",
      callback = function() {
        if (askForCollection && adapter.getEdgeCollections().length > 1) {
          modalDialogHelper.createModalDialog("Select Edge Collection",
            idprefix, [{
              type: "list",
              id: "edge",
              objects: adapter.getEdgeCollections(),
              text: "Select collection",
              selected: adapter.getSelectedEdgeCollection()
            }], function () {
              var edgeCollection = $("#" + idprefix + "edge")
                  .children("option")
                  .filter(":selected")
                  .text();
              adapter.useEdgeCollection(edgeCollection);
            },
            "Select"
          );
        }
        self.rebindAll(self.connectNodesRebinds());
      };
    createIcon(icon, "connect", callback);
  };

  this.addAll = function () {
    self.addControlExpand();
    self.addControlDrag();
    //self.addControlView();
    self.addControlEdit();
    self.addControlConnect();
    self.addControlNewNode();
    self.addControlDelete();
  };
}
