/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, $, _ */
/*global EventDispatcherControls, NodeShaperControls, EdgeShaperControls */
/*global LayouterControls, ArangoAdapterControls*/
/*global GraphViewer, d3, window*/
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

function GraphViewerUI(container, adapterConfig, optWidth, optHeight, viewerConfig) {
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
  
  var graphViewer,
    width = (optWidth || container.offsetWidth) - 60,
    height = optHeight || container.offsetHeight,
    menubar = document.createElement("ul"),
    background = document.createElement("div"),
    colourList,
    nodeShaperUI,
    adapterUI,
    //mousePointerBox = document.createElement("div"),
    svg,
    
    makeBootstrapDropdown = function (div, id, title) {
      var btn, caret, list;
      div.className = "btn-group";
      btn = document.createElement("button");
      btn.className = "btn btn-inverse btn-small dropdown-toggle";
      btn.id = id;
      btn.setAttribute("data-toggle", "dropdown");
      btn.appendChild(document.createTextNode(title + " "));
      caret = document.createElement("span");
      caret.className = "caret";
      btn.appendChild(caret);
      list = document.createElement("ul");
      list.className = "dropdown-menu";
      div.appendChild(btn);
      div.appendChild(list);
      return list;
    },
    createSVG = function () {
      return d3.select("#" + container.id + " #background")
        .append("svg")
        .attr("id", "graphViewerSVG")
        .attr("width",width)
        .attr("height",height)
        .attr("class", "pull-right graphViewer")
        .attr("style", "width:" + width + "px;height:" + height + ";");
    },
    createToolbox = function() {
      var toolbox = document.createElement("div"),
        dispatcherUI = new EventDispatcherControls(
          toolbox,
          //mousePointerBox,
          graphViewer.nodeShaper,
          graphViewer.edgeShaper,
          graphViewer.dispatcherConfig
        );
      toolbox.id = "toolbox";
      toolbox.className = "btn-group btn-group-vertical pull-left toolbox";
      background.appendChild(toolbox);
      /*
      mousePointerBox.id = "mousepointer";
      mousePointerBox.className = "mousepointer";
      background.appendChild(mousePointerBox);
      */
      dispatcherUI.addAll();
    },
    createMenu = function() {
      var transparentHeader = document.createElement("div"),
        searchDiv = document.createElement("div"),
        searchAttrField = document.createElement("input"),
        searchValueField = document.createElement("input"),
        searchStart = document.createElement("img"),
        buttons = document.createElement("div"),
        equalsField = document.createElement("span"),
        configureDropDown = document.createElement("div"),
        configureList = makeBootstrapDropdown(
          configureDropDown,
          "configuredropdown",
          "Configure"
        ),
        
        /*
        nodeShaperDropDown = document.createElement("div"),
        nodeShaperList = makeBootstrapDropdown(
          nodeShaperDropDown,
          "nodeshaperdropdown",
          "Nodes"
        ),
        edgeShaperDropDown = document.createElement("div"),
        edgeShaperList = makeBootstrapDropdown(
          edgeShaperDropDown,
          "edgeshaperdropdown",
          "Edges"
        ),
        adapterDropDown = document.createElement("div"),
        adapterList = makeBootstrapDropdown(
          adapterDropDown,
          "adapterdropdown",
          "Connection"
        ),
        layouterDropDown = document.createElement("div"),
        layouterList = makeBootstrapDropdown(
          layouterDropDown,
          "layouterdropdown",
          "Layout"
        ),
        nodeShaperUI = new NodeShaperControls(
          nodeShaperList,
          graphViewer.nodeShaper
        ),
        edgeShaperUI = new EdgeShaperControls(
          edgeShaperList,
          graphViewer.edgeShaper
        ),
        layouterUI = new LayouterControls(
          layouterList,
          graphViewer.layouter
        ),
        adapterUI = new ArangoAdapterControls(
          adapterList,
          graphViewer.adapter
        ),
        */
        
        searchFunction = function() {
          if (searchAttrField.value === ""
            || searchAttrField.value === undefined) {
            graphViewer.loadGraph(searchValueField.value);
          } else {
            graphViewer.loadGraphWithAttributeValue(
              searchAttrField.value,
              searchValueField.value
            );
          }
        };

      nodeShaperUI = new NodeShaperControls(
        configureList,
        graphViewer.nodeShaper
      );
      adapterUI = new ArangoAdapterControls(
        configureList,
        graphViewer.adapter
      );

      menubar.id = "menubar";
      menubar.className = "thumbnails2";
      
      transparentHeader.id = "transparentHeader";
      
      buttons.id = "modifiers";
      buttons.className = "pull-right";
      
      searchDiv.id = "transparentPlaceholder";
      searchDiv.className = "pull-left";
      
      searchAttrField.id = "attribute";
      searchAttrField.className = "input-mini searchByAttribute";
      searchAttrField.type = "text";
      searchAttrField.placeholder = "key";
      searchValueField.id = "value";
      searchValueField.className = "searchInput";
      searchValueField.type = "text";
      searchValueField.placeholder = "value";
      searchStart.id = "loadnode";
      searchStart.className = "searchSubmit";
      searchStart.width = 16;
      searchStart.height = 16;
      searchStart.src = "img/enter_icon.png";
      
      equalsField.className = "searchEqualsLabel";
      equalsField.appendChild(document.createTextNode("=="));
      
      
      /*
      nodeShaperDropDown.id = "nodeshapermenu";
      edgeShaperDropDown.id = "edgeshapermenu";
      layouterDropDown.id = "layoutermenu";
      adapterDropDown.id = "adaptermenu";
      */
      configureDropDown.id = "configuremenu";
      
      
      searchStart.onclick = searchFunction;
      $(searchValueField).keypress(function(e) {
        if (e.keyCode === 13 || e.which === 13) {
          searchFunction();
          return false;
        }
        
      });
      
      menubar.appendChild(transparentHeader);
      transparentHeader.appendChild(searchDiv);
      searchDiv.appendChild(searchAttrField);
      searchDiv.appendChild(equalsField);
      searchDiv.appendChild(searchValueField);
      searchDiv.appendChild(searchStart);
      transparentHeader.appendChild(buttons);
      
      buttons.appendChild(configureDropDown);
      
      adapterUI.addControlChangeCollections();
      nodeShaperUI.addControlOpticLabelAndColour();
      
      /*
      buttons.appendChild(nodeShaperDropDown);
      buttons.appendChild(edgeShaperDropDown);
      buttons.appendChild(layouterDropDown);
      buttons.appendChild(adapterDropDown);
      
      nodeShaperUI.addAll();
      edgeShaperUI.addAll();
      layouterUI.addAll();
      adapterUI.addAll();
      */
      
    },
    
    createColourList = function() {
      colourList = nodeShaperUI.createColourMappingList();
      colourList.style.position = "absolute";
      var intSVG = $("#graphViewerSVG");
      colourList.style.top = intSVG.position().top.toFixed(1) + "px";
      colourList.style.left = (intSVG.position().left + intSVG.width()).toFixed(1) + "px";
      container.appendChild(colourList);
    };
  container.appendChild(menubar);
  container.appendChild(background);
  background.className = "thumbnails";
  background.id = "background";
  svg = createSVG();
  viewerConfig = viewerConfig || {};
  viewerConfig.zoom = true;
  graphViewer = new GraphViewer(svg, width, height, adapterConfig, viewerConfig);
    
  createToolbox();
  createMenu();
  createColourList();
}