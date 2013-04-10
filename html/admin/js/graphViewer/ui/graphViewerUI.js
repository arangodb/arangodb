/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, $, _ */
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

function GraphViewerUI(container, adapterConfig) {
  "use strict";
  
  if (container === undefined) {
    throw "A parent element has to be given.";
  }
  if (!container.id) {
    throw "The parent element needs an unique id.";
  }
  if (adapterConfig === undefined) {
    throw "An adapter configuration has to be given";
  } 
  
  var makeBootstrapDropdown = function (div, id, title) {
      var btn, caret, list;
      div.className = "dropdown";
      btn = document.createElement("a");
      btn.className = "dropdown-toggle";
      btn.id = id;
      btn.role = "button";
      btn["data-toggle"] = "dropdown";
      btn["data-target"] = "#";
      btn.appendChild(document.createTextNode(title));
      caret = document.createElement("b");
      caret.className = "caret";
      btn.appendChild(caret);
      list = document.createElement("ul");
      list.className = "dropdown-menu";
      list.role = "menu";
      list["aria-labelledby"] = id;
      div.appendChild(btn);
      div.appendChild(list);
      return list;
    },
    createSVG = function () {
      var svg = document.createElement("svg");
      container.appendChild(svg);
    },
    createToolbox = function() {
      var toolbox = document.createElement("div"),
      toollist = document.createElement("ul"),
      dispatcherUI = new EventDispatcherControls(toollist, graphViewer.nodeShaper, graphViewer.edgeShaper, graphViewer.dispatcherConfig);
      toolbox.id = "toolbox";
      toolbox.className = "toolbox";
      container.appendChild(toolbox);
      toolbox.appendChild(toollist);
      dispatcherUI.addAll();
    },
    createMenu = function() {
      var menucontainer = document.createElement("div"),
      menubar = document.createElement("ul"),
      searchDiv = document.createElement("div"),
      searchField = document.createElement("input"),
      searchStart = document.createElement("img"),
      nodeShaperDropDown = document.createElement("div"),
      nodeShaperList = makeBootstrapDropdown(nodeShaperDropDown, "nodeshaperdropdown", "Node Shaper"),
      edgeShaperDropDown = document.createElement("div"),
      edgeShaperList = makeBootstrapDropdown(edgeShaperDropDown, "edgeshaperdropdown", "Edge Shaper"),
      nodeShaperUI = new NodeShaperControls(nodeShaperList, graphViewer.nodeShaper),
      edgeShaperUI = new EdgeShaperControls(edgeShaperList, graphViewer.edgeShaper);
      
      menubar.id = "menubar";
      
      searchField.id = "nodeid";
      searchField.className = "searchInput";
      searchStart.id = "loadnode";
      searchStart.width = 16;
      searchStart.height = 16;
      searchStart.src = "img/enter_icon.png";
      
      nodeShaperDropDown.id = "nodeshapermenu";
      edgeShaperDropDown.id = "edgeshapermenu";
      
      searchStart.onclick = function() {
        var nodeId = searchField.value;
        graphViewer.loadGraph(nodeId);
      };
      
      container.appendChild(menucontainer);
      menucontainer.appendChild(menubar);
      menubar.appendChild(searchDiv);
      searchDiv.appendChild(searchField);
      searchDiv.appendChild(searchStart);
      menubar.appendChild(nodeShaperDropDown);
      menubar.appendChild(edgeShaperDropDown);
      
      
      nodeShaperUI.addAll();
      edgeShaperUI.addAll();
      
    },
    graphViewer,
    width = container.width, // TODO
    height = container.height; // TODO

  createSVG();
  graphViewer = new GraphViewer(d3.select("#" + container.id + " svg"), 10, 10, adapterConfig);
  
  createToolbox();
  createMenu();
}