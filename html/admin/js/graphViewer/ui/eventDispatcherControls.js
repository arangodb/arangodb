/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document, window*/
/*global modalDialogHelper, uiComponentsHelper */
/*global EventDispatcher, EventLibrary*/
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
/* Archive
function EventDispatcherControls(list, cursorIconBox, nodeShaper, edgeShaper, dispatcherConfig) {
*/
function EventDispatcherControls(list, nodeShaper, edgeShaper, dispatcherConfig) {
  "use strict";
  
  if (list === undefined) {
    throw "A list element has to be given.";
  }
  /*
  if (cursorIconBox === undefined) {
    throw "The cursor decoration box has to be given.";
  }
  */
  if (nodeShaper === undefined) {
    throw "The NodeShaper has to be given.";
  }
  if (edgeShaper === undefined) {
    throw "The EdgeShaper has to be given.";
  }
  
  var self = this,
    /* archive
    firstButton = true, 
    currentListGroup,
    placeHolderBtn = uiComponentsHelper.createIconButton(
      "none",
      ""
    ),
    icons = {
      expand: "plus",
      add: "plus-sign",
      trash: "trash",
      drag: "move",
      edge: "resize-horizontal",
      edit: "pencil"
    },
    */
    icons = {
      expand: "expand",
      add: "add",
      trash: "trash",
      drag: "drag",
      edge: "connect",
      edit: "edit"
    },
    baseClass = "event",
    eventlib = new EventLibrary(),
    dispatcher = new EventDispatcher(nodeShaper, edgeShaper, dispatcherConfig),
    
    setCursorIcon = function(icon) {
      //cursorIconBox.className = "mousepointer icon-" + icon;
    },
    
    appendToList = function(button) {
      list.appendChild(button);
      /* archive
      if (firstButton) {
        currentListGroup = document.createElement("div");
        currentListGroup.className = "btn btn-group";
        currentListGroup.appendChild(button);
        currentListGroup.appendChild(placeHolderBtn);
        firstButton = false;
        list.appendChild(currentListGroup);
      } else {
        currentListGroup.removeChild(placeHolderBtn);
        currentListGroup.appendChild(button);
        firstButton = true;
      }
      */
    },
    createButton = function(title, callback) {
      uiComponentsHelper.createButton(
        baseClass,
        list,
        title,
        "control_event_" + title,
        callback
      );
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
      var pos = getCursorPosition(ev);
      pos.x -= $('svg').offset().left;
      pos.y -= $('svg').offset().top;
      return pos;
    };
    /* Archive
    moveCursorBox = function(ev) {
      var pos = getCursorPosition(ev);
      pos.x += 7;
      pos.y += 12;
      cursorIconBox.style.position = "absolute";
      cursorIconBox.style.left  = pos.x + 'px';
      cursorIconBox.style.top = pos.y + 'px';
    };
    */
  /* Archive
  dispatcher.fixSVG("mousemove", moveCursorBox);
  dispatcher.fixSVG("mouseout", function() {
    cursorIconBox.style.display = "none";
  });
  dispatcher.fixSVG("mouseover", function() {
    cursorIconBox.style.display = "block";
  });
  */
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
    var prefix = "control_event_new_node",
      idprefix = prefix + "_",
      createCallback = function(n) {
        modalDialogHelper.createModalCreateDialog(
          "Create New Node",
          idprefix,
          {},
          function(data) {
            dispatcher.events.CREATENODE(data, function() {
              $("#" + idprefix + "modal").modal('hide');
            })();
          }
        );
      };
    return {
      svg: {
        click: createCallback
      }
    };
  };
  
  this.connectNodesRebinds = function() {
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
        dispatcher.bind("svg", "mousemove", function(){});
      }),
      svgUp = function() {
        dispatcher.events.CANCELCREATEEDGE();
        edgeShaper.removeCursorFollowingEdge();
      };
    return {
      nodes: {
        mousedown: nodesDown,
        mouseup: nodesUp
      },
      svg: {
        mouseup: svgUp
      }
    };
  };
  
  this.editRebinds = function() {
    var prefix = "control_event_edit",
      idprefix = prefix + "_",
      nodeCallback = function(n) {
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
      },
      edgeCallback = function(e) {
        modalDialogHelper.createModalEditDialog(
          "Edit Edge " + e._data._from + "->" + e._data._to,
          "control_event_edge_edit_",
          e._data,
          function(newData) {
            dispatcher.events.PATCHEDGE(e, newData, function() {
              $("#control_event_edge_edit_modal").modal('hide');
            })();
          }
        );
      };
      return {
        nodes: {
          click: nodeCallback
        },
        edges: {
          click: edgeCallback
        }
      };
  };
  
  this.expandRebinds = function() {
    return {
      nodes: {
        click: dispatcher.events.EXPAND
      }
    };
  };
  
  this.deleteRebinds = function() {
    return {
      nodes: {
        click: dispatcher.events.DELETENODE(
          function() {}
        )
      },
      edges: {
        click: dispatcher.events.DELETEEDGE(
          function() {}
        )
      }
    };
  };
  
  this.rebindAll = function(obj) {
    rebindNodes(obj.nodes);
    rebindEdges(obj.edges);
    rebindSVG(obj.svg);
  };
  
  /*******************************************
  * Functions to add controls
  *
  *******************************************/
  
  
  this.addControlNewNode = function() {
    var icon = icons.add,
      callback = function() {
        setCursorIcon(icon);
        self.rebindAll(self.newNodeRebinds());
      };
    createIcon(icon, "new_node", callback);
  };
  
  this.addControlDrag = function() {
    var prefix = "control_event_drag",
      idprefix = prefix + "_",
      icon = icons.drag,
      callback = function() {
        setCursorIcon(icon);
        self.rebindAll(self.dragRebinds());
      };
    createIcon(icon, "drag", callback);
  };
  
  this.addControlEdit = function() {
    var icon = icons.edit,
      callback = function() {
        setCursorIcon(icon);
        self.rebindAll(self.editRebinds());
      };
    createIcon(icon, "edit", callback);
  };
  
  this.addControlExpand = function() {
    var icon = icons.expand,
      callback = function() {
        setCursorIcon(icon);
        self.rebindAll(self.expandRebinds());
      };
    createIcon(icon, "expand", callback);
  };
  
  this.addControlDelete = function() {
    var icon = icons.trash,
      callback = function() {
        setCursorIcon(icon);
        rebindNodes({click: dispatcher.events.DELETENODE(function() {
          
        })});
        rebindEdges({click: dispatcher.events.DELETEEDGE(function() {
          
        })});
        rebindSVG();
      };
    createIcon(icon, "delete", callback);
  };
  
  this.addControlConnect = function() {
    var icon = icons.edge,
      callback = function() {
        setCursorIcon(icon);
        self.rebindAll(self.connectNodesRebinds());
      };
    createIcon(icon, "connect", callback);
  };
  
  this.addAll = function () {
    self.addControlDrag();
    self.addControlEdit();
    self.addControlExpand();
    self.addControlDelete();
    self.addControlConnect();
    self.addControlNewNode();
  };
  
}