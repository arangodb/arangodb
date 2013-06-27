/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, $, _ */
/*global d3, window*/
/*global GraphViewer, EventDispatcherControls, EventDispatcher, NodeShaper */
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

function GraphViewerPreview(container, viewerConfig) {
  "use strict";
  
  /*******************************************************************************
  * Internal variables and functions
  *******************************************************************************/

  var svg,
    width,
    height,
    viewer,
    createTB,
    adapterConfig,
    dispatcherUI,
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
    
    shouldCreateToolbox = function(config) {
      var counter = 0;
      _.each(config, function(v, k) {
        if (v === false) {
          delete config[k];
        } else {
          counter++;
        }
      });
      return counter > 0;
    },
    
    addRebindsToList = function(list, rebinds) {
      _.each(rebinds, function(acts, obj) {
        list[obj] = list[obj] || {};
        _.each(acts, function(func, trigger) {
          list[obj][trigger] = func;
        });
      });
    },
    
    parseActions = function(config) {
      if (!config) {
        return;
      }
      var allActions = {};
      if (config.drag) {
        addRebindsToList(allActions, dispatcherUI.dragRebinds());
      }
      if (config.create) {
        addRebindsToList(allActions, dispatcherUI.newNodeRebinds());
        addRebindsToList(allActions, dispatcherUI.connectNodesRebinds());
      }
      if (config.remove) {
        addRebindsToList(allActions, dispatcherUI.deleteRebinds());
      }
      if (config.expand) {
        addRebindsToList(allActions, dispatcherUI.expandRebinds());
      }
      if (config.edit) {
        addRebindsToList(allActions, dispatcherUI.editRebinds());
      }
      dispatcherUI.rebindAll(allActions);      
    },
    
    createToolbox = function(config) {
      var toolbox = document.createElement("div");
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
      _.each(config, function(v, k) {
        switch(k) {
          case "expand":
            dispatcherUI.addControlExpand();
            break;
          case "create":
            dispatcherUI.addControlNewNode();
            dispatcherUI.addControlConnect();
            break;
          case "drag":
            dispatcherUI.addControlDrag();
            break;
          case "edit":
            dispatcherUI.addControlEdit();
            break;
          case "remove":
            dispatcherUI.addControlDelete();
            break;
        }
      });
    },
    
    createDispatcherOnly = function(config) {
      var toolbox = document.createElement("div");
      dispatcherUI = new EventDispatcherControls(
        toolbox,
        mousePointerBox,
        viewer.nodeShaper,
        viewer.edgeShaper,
        viewer.dispatcherConfig
      );
    },
    
    changeConfigToPreviewGraph = function() {
      if (viewerConfig) {
        // Fix nodeShaper:
        if (viewerConfig.nodeShaper) {
          if (viewerConfig.nodeShaper.label) {
            viewerConfig.nodeShaper.label = "label";
          }
          if (
            viewerConfig.nodeShaper.shape
            && viewerConfig.nodeShaper.shape.type === NodeShaper.shapes.IMAGE
            && viewerConfig.nodeShaper.shape.source
          ) {
            viewerConfig.nodeShaper.shape.source = "image";
          }
        }
        // Fix nodeShaper:
        if (viewerConfig.edgeShaper) {
          if (viewerConfig.edgeShaper.label) {
            viewerConfig.edgeShaper.label = "label";
          }
        }
      }
    },
    
    createViewer = function() {
      changeConfigToPreviewGraph();
      return new GraphViewer(svg, width, height, adapterConfig, viewerConfig);
    };
  
  
  /*******************************************************************************
  * Execution start
  *******************************************************************************/  
  
  width = container.offsetWidth;
  height = container.offsetHeight;
  adapterConfig = {
    type: "preview"
  };
  
  viewerConfig = viewerConfig || {};
  createTB = shouldCreateToolbox(viewerConfig.toolbox);
  if (createTB) {
    width -= 43;
  }
  svg = createSVG();
  viewer = createViewer();
  if (createTB) {
    createToolbox(viewerConfig.toolbox);
  } else {
    createDispatcherOnly();
  }
  viewer.loadGraph("1");
  parseActions(viewerConfig.actions);
  
}