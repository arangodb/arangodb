/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, $, _ */
/*global d3, window*/
/*global GraphViewer, EventDispatcherControls */
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

/*******************************************************************************
* Assume the widget is imported via an iframe.
* Hence we append everything directly to the body
* and make use of all available space.
*******************************************************************************/

function GraphViewerWidget(viewerConfig, startNode) {
  "use strict";
  
  /*******************************************************************************
  * Internal variables and functions
  *******************************************************************************/

  var svg,
    container,
    width,
    height,
    viewer,
    adapterConfig,
    mousePointerBox = document.createElement("div"),
    
    
    createSVG = function() {
      return d3.select(container)
        .append("svg")
        .attr("id", "graphViewerSVG")
        .attr("width",width)
        .attr("height",height)
        .attr("class", "graphViewer")
        .attr("style", "width:" + width + "px;height:" + height + ";");
    },
    
    createToolbox = function(config) {
      var toolbox = document.createElement("div"),
        dispatcherUI = new EventDispatcherControls(
          toolbox,
          mousePointerBox,
          viewer.nodeShaper,
          viewer.edgeShaper,
          viewer.dispatcherConfig
        );
      toolbox.id = "toolbox";
      toolbox.className = "btn-group btn-group-vertical pull-left toolbox";
      mousePointerBox.id = "mousepointer";
      mousePointerBox.className = "mousepointer";
      container.appendChild(toolbox);
      container.appendChild(mousePointerBox);
      dispatcherUI.addAll();
    },
    
    
    createViewer = function() {
      return new GraphViewer(svg, width, height, adapterConfig, viewerConfig);
    };
  
  
  /*******************************************************************************
  * Execution start
  *******************************************************************************/  
  
  container = document.body;
  width = container.offsetWidth;
  height = container.offsetHeight;
  adapterConfig = {
    type: "foxx",
    route: "."
  };
  
  viewerConfig = viewerConfig || {};
  
  svg = createSVG();
  viewer = createViewer();
  if (viewerConfig.toolbox) {
    createToolbox(viewerConfig.toolbox);
  }
  if (startNode) {
    viewer.loadGraph(startNode);
  }
  
  
}