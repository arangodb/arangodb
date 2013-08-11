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

function GraphViewerUI(container, adapterConfig, optWidth, optHeight, viewerConfig, startNode) {
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
    slider,
    searchAttrField,
    searchAttrExampleList,
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
        .attr("class", "graphViewer pull-right")
        .attr("style", "width:" + width + "px;height:" + height + "px;");
    },
    
    createZoomUIWidget = function() {
      var zoomUI = document.createElement("div"),
        zoomButtons = document.createElement("div"),
        btnTop = document.createElement("button"),
        btnLeft = document.createElement("button"),
        btnRight = document.createElement("button"),
        btnBottom = document.createElement("button");
      zoomUI.className = "gv_zoom_widget";
      zoomButtons.className = "gv_zoom_buttons_bg";
      
      btnTop.className = "btn btn-icon btn-zoom btn-zoom-top gv-zoom-btn pan-top";
      btnLeft.className = "btn btn-icon btn-zoom btn-zoom-left gv-zoom-btn pan-left";
      btnRight.className = "btn btn-icon btn-zoom btn-zoom-right gv-zoom-btn pan-right";
      btnBottom.className = "btn btn-icon btn-zoom btn-zoom-bottom gv-zoom-btn pan-bottom";
      btnTop.onclick = function() {
        graphViewer.zoomManager.triggerTranslation(0, -10);
      };
      btnLeft.onclick = function() {
        graphViewer.zoomManager.triggerTranslation(-10, 0);
      };
      btnRight.onclick = function() {
        graphViewer.zoomManager.triggerTranslation(10, 0);
      };
      btnBottom.onclick = function() {
        graphViewer.zoomManager.triggerTranslation(0, 10);
      };
      
      zoomButtons.appendChild(btnTop);
      zoomButtons.appendChild(btnLeft);
      zoomButtons.appendChild(btnRight);
      zoomButtons.appendChild(btnBottom);
      
      slider = document.createElement("div");
      slider.id = "gv_zoom_slider";
      slider.className = "gv_zoom_slider";
      
      background.appendChild(zoomUI);
      zoomUI.appendChild(zoomButtons);
      
      zoomUI.appendChild(slider);
      $( "#gv_zoom_slider" ).slider({
        orientation: "vertical",
        min: graphViewer.zoomManager.getMinimalZoomFactor(),
        max: 1,
        value: 1,
        step: 0.01,
        slide: function( event, ui ) {
          graphViewer.zoomManager.triggerScale(ui.value);
        }
      });
      graphViewer.zoomManager.registerSlider($("#gv_zoom_slider"));
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
    
    updateAttributeExamples = function() {
      searchAttrExampleList.innerHTML = "";
      var throbber = document.createElement("li"),
        throbberImg = document.createElement("img");
      throbber.appendChild(throbberImg);
      throbberImg.className = "gv-throbber";
      searchAttrExampleList.appendChild(throbber);
      graphViewer.adapter.getAttributeExamples(function(res) {
        searchAttrExampleList.innerHTML = "";
        _.each(res, function(r) {
          var entry = document.createElement("li"),
            link = document.createElement("a"),
            lbl = document.createElement("label");
          entry.appendChild(link);
          link.appendChild(lbl);
          lbl.appendChild(document.createTextNode(r));
          searchAttrExampleList.appendChild(entry);
          entry.onclick = function() {
            searchAttrField.value = r;
          };
        });
      });
    },
    
    createMenu = function() {
      var transparentHeader = document.createElement("div"),
        searchDiv = document.createElement("div"),
        searchAttrDiv = document.createElement("div"),
        searchAttrExampleToggle = document.createElement("button"),
        searchAttrExampleCaret = document.createElement("span"),
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
        
        showSpinner = function() {
          $(background).css("cursor", "progress");
        },
        
        hideSpinner = function() {
          $(background).css("cursor", "");
        },
        
        alertError = function(msg) {
          window.alert(msg);
        },
        
        resultCB = function(res) {
          hideSpinner();
          if (res && res.errorCode && res.errorCode === 404) {
            alertError("Sorry could not find a matching node.");
            return;
          }
          return;
        },
        
        searchFunction = function() {
          showSpinner();
          if (searchAttrField.value === ""
            || searchAttrField.value === undefined) {
            graphViewer.loadGraph(
              searchValueField.value,
              resultCB
            );
          } else {
            graphViewer.loadGraphWithAttributeValue(
              searchAttrField.value,
              searchValueField.value,
              resultCB
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
      
      searchAttrField = document.createElement("input");
      searchAttrExampleList = document.createElement("ul");
      
      menubar.id = "menubar";
      menubar.className = "thumbnails2";
      
      transparentHeader.id = "transparentHeader";
      
      buttons.id = "modifiers";
      buttons.className = "pull-right";
      
      searchDiv.id = "transparentPlaceholder";
      searchDiv.className = "pull-left";
      
      searchAttrDiv.className = "pull-left input-append searchByAttribute";
      searchAttrField.id = "attribute";
      searchAttrField.className = "input";
      searchAttrField.type = "text";
      searchAttrField.placeholder = "Attribute";
      searchAttrExampleToggle.id = "attribute_example_toggle";
      searchAttrExampleToggle.className = "btn gv_example_toggle";
      searchAttrExampleToggle.setAttribute("data-toggle", "dropdown");
      searchAttrExampleCaret.className = "caret";
      searchAttrExampleList.className = "dropdown-menu";
      searchValueField.id = "value";
      searchValueField.className = "input-xlarge searchInput";
      searchValueField.type = "text";
      searchValueField.placeholder = "Value";
      searchStart.id = "loadnode";
      searchStart.className = "searchSubmit";
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
      searchDiv.appendChild(searchAttrDiv);
      searchAttrDiv.appendChild(searchAttrField);
      searchAttrDiv.appendChild(searchAttrExampleToggle);
      searchAttrDiv.appendChild(searchAttrExampleList);
      searchAttrExampleToggle.appendChild(searchAttrExampleCaret);
      searchDiv.appendChild(equalsField);
      searchDiv.appendChild(searchValueField);
      searchDiv.appendChild(searchStart);
      transparentHeader.appendChild(buttons);
      
      buttons.appendChild(configureDropDown);
      
      adapterUI.addControlChangeCollections(updateAttributeExamples);
      adapterUI.addControlChangePriority();
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
      updateAttributeExamples();
    },
    
    createColourList = function() {
      colourList = nodeShaperUI.createColourMappingList();
      colourList.style.position = "absolute";
      var intSVG = $("#graphViewerSVG");
      colourList.style.top = intSVG.position().top.toFixed(1) + "px";
      colourList.style.left = (intSVG.position().left + intSVG.width()).toFixed(1) + "px";
      colourList.style.height = intSVG.height() + "px";
      colourList.style.overflow = "auto";
      container.appendChild(colourList);
    };
  container.appendChild(menubar);
  container.appendChild(background);
  background.className = "thumbnails";
  background.id = "background";
  
  viewerConfig = viewerConfig || {};
  viewerConfig.zoom = true;

  
  svg = createSVG();
  graphViewer = new GraphViewer(svg, width, height, adapterConfig, viewerConfig);

  createToolbox();
  createZoomUIWidget();
  createMenu();
  createColourList();

  if (startNode) {
    graphViewer.loadGraph(startNode);
  }

}
