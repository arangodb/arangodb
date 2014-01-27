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
    width = (optWidth || container.offsetWidth) - 81,
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

    makeFilterDiv = function() {
      var 
        div = document.createElement("div"),
        innerDiv = document.createElement("div"),
        queryLine = document.createElement("div"),
        searchAttrDiv = document.createElement("div"),
        searchAttrExampleToggle = document.createElement("button"),
        searchAttrExampleCaret = document.createElement("span"),
        searchValueField = document.createElement("input"),
        searchStart = document.createElement("img"),
        equalsField = document.createElement("span"),

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

      div.id = "filterDropdown";
      div.className = "headerDropdown";
      innerDiv.className = "dropdownInner";
      queryLine.className = "queryline";

      searchAttrField = document.createElement("input");
      searchAttrExampleList = document.createElement("ul");

      searchAttrDiv.className = "pull-left input-append searchByAttribute";
      searchAttrField.id = "attribute";
      //searchAttrField.className = "input";
      searchAttrField.type = "text";
      searchAttrField.placeholder = "Attribute name";
      searchAttrExampleToggle.id = "attribute_example_toggle";
      searchAttrExampleToggle.className = "btn gv_example_toggle";
      searchAttrExampleCaret.className = "caret gv_caret";
      searchAttrExampleList.className = "dropdown-menu";
      searchValueField.id = "value";
      searchValueField.className = "searchInput";
      //searchValueField.className = "filterValue";
      searchValueField.type = "text";
      searchValueField.placeholder = "Attribute value";
      searchStart.id = "loadnode";
      searchStart.className = "searchSubmit";
      equalsField.className = "searchEqualsLabel";
      equalsField.appendChild(document.createTextNode("=="));

      innerDiv.appendChild(queryLine);
      queryLine.appendChild(searchAttrDiv);

      searchAttrDiv.appendChild(searchAttrField);
      searchAttrDiv.appendChild(searchAttrExampleToggle);
      searchAttrDiv.appendChild(searchAttrExampleList);
      searchAttrExampleToggle.appendChild(searchAttrExampleCaret);
      queryLine.appendChild(equalsField);
      queryLine.appendChild(searchValueField);
      queryLine.appendChild(searchStart);

      searchStart.onclick = searchFunction;
      $(searchValueField).keypress(function(e) {
        if (e.keyCode === 13 || e.which === 13) {
          searchFunction();
          return false;
        }
      });
      
      searchAttrExampleToggle.onclick = function() {
        $(searchAttrExampleList).slideToggle(200);
      };

      div.appendChild(innerDiv);
      return div;
    },

    makeConfigureDiv = function () {
      var div, innerDiv, nodeList, nodeHeader, colList, colHeader;
      div = document.createElement("div");
      div.id = "configureDropdown";
      div.className = "headerDropdown";
      innerDiv = document.createElement("div");
      innerDiv.className = "dropdownInner";
      nodeList = document.createElement("ul");
      nodeHeader = document.createElement("li");
      nodeHeader.className = "nav-header";
      nodeHeader.appendChild(document.createTextNode("Vertices"));
      colList = document.createElement("ul");
      colHeader = document.createElement("li");
      colHeader.className = "nav-header";
      colHeader.appendChild(document.createTextNode("Connection"));
      nodeList.appendChild(nodeHeader);
      colList.appendChild(colHeader);
      innerDiv.appendChild(nodeList);
      innerDiv.appendChild(colList);
      div.appendChild(innerDiv);
      return {
        configure: div,
        nodes: nodeList,
        col: colList
      };
    },

    makeConfigure = function (div, idConf, idFilter) {
      var ul, liConf, aConf, spanConf, liFilter, aFilter, spanFilter, lists;
      div.className = "headerButtonBar pull-right";
      ul = document.createElement("ul");
      ul.className = "headerButtonList";

      div.appendChild(ul);

      liFilter = document.createElement("li");
      liFilter.className = "enabled";
      aFilter = document.createElement("a");
      aFilter.id = idFilter;
      aFilter.className = "headerButton";
      spanFilter = document.createElement("span");
      spanFilter.className = "icon_arangodb_filter";
      $(spanFilter).attr("title", "Filter");
      
      ul.appendChild(liFilter);
      liFilter.appendChild(aFilter);
      aFilter.appendChild(spanFilter);

      liConf = document.createElement("li");
      liConf.className = "enabled";
      aConf = document.createElement("a");
      aConf.id = idConf;
      aConf.className = "headerButton";
      spanConf = document.createElement("span");
      spanConf.className = "icon_arangodb_settings2";
      $(spanConf).attr("title", "Configure");

      ul.appendChild(liConf);
      liConf.appendChild(aConf);
      aConf.appendChild(spanConf);

      lists = makeConfigureDiv();
      lists.filter = makeFilterDiv();
      aConf.onclick = function () {
        $(lists.configure).slideToggle(200);
        $(lists.filter).hide();
      };

      aFilter.onclick = function () {
        $(lists.filter).slideToggle(200);
        $(lists.configure).hide();
      };

      return lists;
    },

    createSVG = function () {
      return d3.select("#" + container.id + " #background")
        .append("svg")
        .attr("id", "graphViewerSVG")
        .attr("width",width)
        .attr("height",height)
        .attr("class", "graphViewer")
        .style("width", width + "px")
        .style("height", height + "px");
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
      background.insertBefore(zoomUI, svg[0][0]);
      
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
          graphViewer.nodeShaper,
          graphViewer.edgeShaper,
          graphViewer.dispatcherConfig
        );
      toolbox.id = "toolbox";
      toolbox.className = "btn-group btn-group-vertical toolbox";
      background.insertBefore(toolbox, svg[0][0]);
      dispatcherUI.addAll();
      // Default selection
      $("#control_event_expand").click();
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
          lbl.className = "gv_dropdown_label";
          searchAttrExampleList.appendChild(entry);
          entry.onclick = function() {
            searchAttrField.value = r;
            $(searchAttrExampleList).slideToggle(200);
          };
        });
      });
    },
    
    createMenu = function() {
      var transparentHeader = document.createElement("div"),
        buttons = document.createElement("div"),
        title = document.createElement("a"),
        configureLists = makeConfigure(
          buttons,
          "configuredropdown",
          "filterdropdown"
        );
        
      nodeShaperUI = new NodeShaperControls(
        configureLists.nodes,
        graphViewer.nodeShaper
      );
      adapterUI = new ArangoAdapterControls(
        configureLists.col,
        graphViewer.adapter
      );
      
      menubar.id = "menubar";
      menubar.className = "thumbnails2";
      
      transparentHeader.className = "headerBar";
      
      buttons.id = "modifiers";

      title.appendChild(document.createTextNode("Graph Viewer"));
      title.className = "arangoHeader";
      
      /*
      nodeShaperDropDown.id = "nodeshapermenu";
      edgeShaperDropDown.id = "edgeshapermenu";
      layouterDropDown.id = "layoutermenu";
      adapterDropDown.id = "adaptermenu";
      */
      
      menubar.appendChild(transparentHeader);
      menubar.appendChild(configureLists.configure);
      menubar.appendChild(configureLists.filter);
      transparentHeader.appendChild(buttons);
      transparentHeader.appendChild(title);
      
      adapterUI.addControlChangeCollections(function() {
        updateAttributeExamples();
        graphViewer.start();
      });
      adapterUI.addControlChangePriority();
      // nodeShaperUI.addControlOpticLabelAndColour(graphViewer.adapter);
      nodeShaperUI.addControlOpticLabelAndColourList(graphViewer.adapter);
      
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
      colourList.className = "gv_colour_list";
      background.insertBefore(colourList, svg[0][0]);
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

  this.changeWidth = function(w) {
    graphViewer.changeWidth(w);
    var reducedW = w - 60;
    svg.attr("width", reducedW)
      .style("width", reducedW + "px");
  };
}
