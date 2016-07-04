/*global document, $, _ */
/*global EventDispatcherControls, NodeShaperControls, EdgeShaperControls */
/*global LayouterControls, GharialAdapterControls*/
/*global GraphViewer, d3, window, arangoHelper, Storage, localStorage*/
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
    width = (optWidth + 20 || container.getBoundingClientRect().width - 81 + 20),
    height = optHeight || container.getBoundingClientRect().height,
    menubar = document.createElement("ul"),
    background = document.createElement("div"),
    colourList,
    nodeShaperUI,
    edgeShaperUI,
    adapterUI,
    slider,
    searchAttrExampleList,
    searchAttrExampleList2,
    //mousePointerBox = document.createElement("div"),
    svg,

    makeDisplayInformationDiv = function() {
      if (graphViewer.adapter.NODES_TO_DISPLAY < graphViewer.adapter.TOTAL_NODES) {
        $('.headerBar').append(
          '<div class="infoField">Graph too big. A random section is rendered.<div class="fa fa-info-circle"></div></div>'
        );
        $('.infoField .fa-info-circle').attr("title", "You can display additional/other vertices by using the toolbar buttons.").tooltip();
      }
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
        searchStart = document.createElement("i"),
        equalsField = document.createElement("span"),
        searchAttrField,

        showSpinner = function() {
          $(background).css("cursor", "progress");
        },

        hideSpinner = function() {
          $(background).css("cursor", "");
        },

        alertError = function(msg) {
          arangoHelper.arangoError("Graph", msg);
        },

        resultCB = function(res) {
          hideSpinner();
          if (res && res.errorCode && res.errorCode === 404) {
            arangoHelper.arangoError("Graph error", "could not find a matching node.");
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
      div.className = "headerDropdown smallDropdown";
      innerDiv.className = "dropdownInner";
      queryLine.className = "queryline";

      searchAttrField = document.createElement("input");
      searchAttrExampleList = document.createElement("ul");

      searchAttrDiv.className = "pull-left input-append searchByAttribute";
      searchAttrField.id = "attribute";
      searchAttrField.type = "text";
      searchAttrField.placeholder = "Attribute name";
      searchAttrExampleToggle.id = "attribute_example_toggle";
      searchAttrExampleToggle.className = "button-neutral gv_example_toggle";
      searchAttrExampleCaret.className = "caret gv_caret";
      searchAttrExampleList.className = "gv-dropdown-menu";
      searchValueField.id = "value";
      searchValueField.className = "searchInput gv_searchInput";
      //searchValueField.className = "filterValue";
      searchValueField.type = "text";
      searchValueField.placeholder = "Attribute value";
      searchStart.id = "loadnode";
      searchStart.className = "fa fa-search";
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

      var title = document.createElement("p");
      title.className = "dropdown-title";
      title.innerHTML = "Filter graph by attribute:";
      div.appendChild(title);

      div.appendChild(innerDiv);
      return div;
    },

    makeNodeDiv = function () {
      var
        div2 = document.createElement("div"),
        innerDiv2 = document.createElement("div"),
        queryLine2 = document.createElement("div"),
        searchAttrDiv2 = document.createElement("div"),
        searchAttrExampleToggle2 = document.createElement("button"),
        searchAttrExampleCaret2 = document.createElement("span"),
        searchValueField2 = document.createElement("input"),
        searchStart2 = document.createElement("i"),
        equalsField2 = document.createElement("span"),
        searchAttrField2,

        showSpinner = function() {
          $(background).css("cursor", "progress");
        },

        hideSpinner = function() {
          $(background).css("cursor", "");
        },

        alertError = function(msg) {
          arangoHelper.arangoError("Graph", msg);
        },

        resultCB2 = function(res) {
          hideSpinner();
          if (res && res.errorCode && res.errorCode === 404) {
            arangoHelper.arangoError("Graph error", "could not find a matching node.");
            return;
          }
          return;
        },

        addCustomNode = function() {
          showSpinner();
          if (searchAttrField2.value !== "") {
            graphViewer.loadGraphWithAdditionalNode(
              searchAttrField2.value,
              searchValueField2.value,
              resultCB2
            );
          }
        };

      div2.id = "nodeDropdown";
      div2.className = "headerDropdown smallDropdown";
      innerDiv2.className = "dropdownInner";
      queryLine2.className = "queryline";

      searchAttrField2 = document.createElement("input");
      searchAttrExampleList2 = document.createElement("ul");

      searchAttrDiv2.className = "pull-left input-append searchByAttribute";
      searchAttrField2.id = "attribute";
      searchAttrField2.type = "text";
      searchAttrField2.placeholder = "Attribute name";
      searchAttrExampleToggle2.id = "attribute_example_toggle2";
      searchAttrExampleToggle2.className = "button-neutral gv_example_toggle";
      searchAttrExampleCaret2.className = "caret gv_caret";
      searchAttrExampleList2.className = "gv-dropdown-menu";
      searchValueField2.id = "value";
      searchValueField2.className = "searchInput gv_searchInput";
      //searchValueField.className = "filterValue";
      searchValueField2.type = "text";
      searchValueField2.placeholder = "Attribute value";
      searchStart2.id = "loadnode";
      searchStart2.className = "fa fa-search";
      equalsField2.className = "searchEqualsLabel";
      equalsField2.appendChild(document.createTextNode("=="));

      innerDiv2.appendChild(queryLine2);
      queryLine2.appendChild(searchAttrDiv2);

      searchAttrDiv2.appendChild(searchAttrField2);
      searchAttrDiv2.appendChild(searchAttrExampleToggle2);
      searchAttrDiv2.appendChild(searchAttrExampleList2);
      searchAttrExampleToggle2.appendChild(searchAttrExampleCaret2);
      queryLine2.appendChild(equalsField2);
      queryLine2.appendChild(searchValueField2);
      queryLine2.appendChild(searchStart2);

      updateAttributeExamples(searchAttrExampleList2);
      //searchAttrExampleList2.onclick = function() {
      //  updateAttributeExamples(searchAttrExampleList2);
      //};

      searchStart2.onclick = addCustomNode;
      $(searchValueField2).keypress(function(e) {
        if (e.keyCode === 13 || e.which === 13) {
          addCustomNode();
          return false;
        }
      });

      searchAttrExampleToggle2.onclick = function() {
        $(searchAttrExampleList2).slideToggle(200);
      };

      var title = document.createElement("p");
      title.className = "dropdown-title";
      title.innerHTML = "Add specific node by attribute:";
      div2.appendChild(title);

      div2.appendChild(innerDiv2);
      return div2;
    },

    makeConfigureDiv = function () {
      var div, innerDiv, nodeList, nodeHeader, colList, colHeader,
          edgeList, edgeHeader;
      div = document.createElement("div");
      div.id = "configureDropdown";
      div.className = "headerDropdown";
      innerDiv = document.createElement("div");
      innerDiv.className = "dropdownInner";
      nodeList = document.createElement("ul");
      nodeHeader = document.createElement("li");
      nodeHeader.className = "nav-header";
      nodeHeader.appendChild(document.createTextNode("Vertices"));
      edgeList = document.createElement("ul");
      edgeHeader = document.createElement("li");
      edgeHeader.className = "nav-header";
      edgeHeader.appendChild(document.createTextNode("Edges"));
      colList = document.createElement("ul");
      colHeader = document.createElement("li");
      colHeader.className = "nav-header";
      colHeader.appendChild(document.createTextNode("Connection"));
      nodeList.appendChild(nodeHeader);
      edgeList.appendChild(edgeHeader);
      colList.appendChild(colHeader);
      innerDiv.appendChild(nodeList);
      innerDiv.appendChild(edgeList);
      innerDiv.appendChild(colList);
      div.appendChild(innerDiv);
      return {
        configure: div,
        nodes: nodeList,
        edges: edgeList,
        col: colList
      };
    },

    makeConfigure = function (div, idConf, idFilter, idNode) {
      var ul, lists,
      liConf, aConf, spanConf,
      liNode, aNode, spanNode,
      liFilter, aFilter, spanFilter;

      div.className = "headerButtonBar";
      ul = document.createElement("ul");
      ul.className = "headerButtonList";

      div.appendChild(ul);

      //CONF
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

      //NODE
      liNode = document.createElement("li");
      liNode.className = "enabled";
      aNode = document.createElement("a");
      aNode.id = idNode;
      aNode.className = "headerButton";
      spanNode = document.createElement("span");
      spanNode.className = "fa fa-search-plus";
      $(spanNode).attr("title", "Show additional vertices");

      ul.appendChild(liNode);
      liNode.appendChild(aNode);
      aNode.appendChild(spanNode);

      //FILTER
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

      lists = makeConfigureDiv();
      lists.filter = makeFilterDiv();
      lists.node = makeNodeDiv();

      aConf.onclick = function () {
        $('#filterdropdown').removeClass('activated');
        $('#nodedropdown').removeClass('activated');
        $('#configuredropdown').toggleClass('activated');
        $(lists.configure).slideToggle(200);
        $(lists.filter).hide();
        $(lists.node).hide();
      };

      aNode.onclick = function () {
        $('#filterdropdown').removeClass('activated');
        $('#configuredropdown').removeClass('activated');
        $('#nodedropdown').toggleClass('activated');
        $(lists.node).slideToggle(200);
        $(lists.filter).hide();
        $(lists.configure).hide();
      };

      aFilter.onclick = function () {
        $('#configuredropdown').removeClass('activated');
        $('#nodedropdown').removeClass('activated');
        $('#filterdropdown').toggleClass('activated');
        $(lists.filter).slideToggle(200);
        $(lists.node).hide();
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
        .attr("class", "graph-viewer")
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
          graphViewer.start,
          graphViewer.dispatcherConfig
        );
      toolbox.id = "toolbox";
      toolbox.className = "btn-group btn-group-vertical toolbox";
      background.insertBefore(toolbox, svg[0][0]);
      dispatcherUI.addAll();
      // Default selection
      $("#control_event_expand").click();


    },

    createOptionBox = function() {
      //create select option box 
      var optionBox = '<li class="enabled" style="float:right">'+
      '<select id="graphSize" class="documents-size">'+
      '<optgroup label="Starting points:">'+
      '<option value="5" selected="">5 vertices</option>'+
      '<option value="10">10 vertices</option>'+
      '<option value="20">20 vertices</option>'+
      '<option value="50">50 vertices</option>'+
      '<option value="100">100 vertices</option>'+
      '<option value="500">500 vertices</option>'+
      '<option value="1000">1000 vertices</option>'+
      '<option value="2500">2500 vertices</option>'+
      '<option value="5000">5000 vertices</option>'+
      '<option value="all">All vertices</option>'+
      '</select>'+
      '</optgroup>'+
      '</li>';
      $('.headerBar .headerButtonList').prepend(optionBox);
    },

    updateAttributeExamples = function(e) {
      var element;

      if (e) {
        element = $(e);
      }
      else {
        element = $(searchAttrExampleList);
      }

      element.innerHTML = "";
      var throbber = document.createElement("li"),
        throbberImg = document.createElement("img");
      $(throbber).append(throbberImg);
      throbberImg.className = "gv-throbber";
      element.append(throbber);
      graphViewer.adapter.getAttributeExamples(function(res) {
        $(element).html('');
        _.each(res, function(r) {
          var entry = document.createElement("li"),
            link = document.createElement("a"),
            lbl = document.createElement("label");
          $(entry).append(link);
          $(link).append(lbl);
          $(lbl).append(document.createTextNode(r));
          lbl.className = "gv_dropdown_label";
          element.append(entry);
          entry.onclick = function() {
            element.value = r;
            $(element).parent().find('input').val(r);
            $(element).slideToggle(200);
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
          "filterdropdown",
          "nodedropdown"
        );

      nodeShaperUI = new NodeShaperControls(
        configureLists.nodes,
        graphViewer.nodeShaper
      );
      edgeShaperUI = new EdgeShaperControls(
        configureLists.edges,
        graphViewer.edgeShaper
      );
      adapterUI = new GharialAdapterControls(
        configureLists.col,
        graphViewer.adapter
      );

      menubar.id = "menubar";

      transparentHeader.className = "headerBar";

      buttons.id = "modifiers";

      //title.appendChild(document.createTextNode("Graph Viewer"));
      //title.className = "arangoHeader";

      /*
      nodeShaperDropDown.id = "nodeshapermenu";
      edgeShaperDropDown.id = "edgeshapermenu";
      layouterDropDown.id = "layoutermenu";
      adapterDropDown.id = "adaptermenu";
      */

      menubar.appendChild(transparentHeader);
      menubar.appendChild(configureLists.configure);
      menubar.appendChild(configureLists.filter);
      menubar.appendChild(configureLists.node);
      transparentHeader.appendChild(buttons);
      //transparentHeader.appendChild(title);

      adapterUI.addControlChangeGraph(function() {
        updateAttributeExamples();
        graphViewer.start(true);
      });
      adapterUI.addControlChangePriority();
      // nodeShaperUI.addControlOpticLabelAndColour(graphViewer.adapter);
      nodeShaperUI.addControlOpticLabelAndColourList(graphViewer.adapter);
      edgeShaperUI.addControlOpticLabelList();

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
      colourList.className = "gv-colour-list";
      background.insertBefore(colourList, svg[0][0]);
    };

  container.appendChild(menubar);
  container.appendChild(background);
  background.className = "contentDiv gv-background ";
  background.id = "background";

  viewerConfig = viewerConfig || {};
  viewerConfig.zoom = true;

  $('#subNavigationBar .breadcrumb').html(
    'Graph: ' + adapterConfig.graphName
  );

  svg = createSVG();

  if (Storage !== "undefined") {
    this.graphSettings = {};

    this.loadLocalStorage = function() {
      //graph name not enough, need to set db name also
      var dbName = adapterConfig.baseUrl.split('/')[2],
      combinedGraphName = adapterConfig.graphName + dbName;
      
      if (localStorage.getItem('graphSettings') === null || localStorage.getItem('graphSettings')  === 'null') {
        var obj = {};
        obj[combinedGraphName] = {
          viewer: viewerConfig,
          adapter: adapterConfig
        };
        localStorage.setItem('graphSettings', JSON.stringify(obj));
      }
      else {
        try {
          var settings = JSON.parse(localStorage.getItem('graphSettings'));
          this.graphSettings = settings;

          if (settings[combinedGraphName].viewer !== undefined) {
            viewerConfig = settings[combinedGraphName].viewer;  
          }
          if (settings[combinedGraphName].adapter !== undefined) {
            adapterConfig = settings[combinedGraphName].adapter;
          }
        }
        catch (e) {
          console.log("Could not load graph settings, resetting graph settings.");
          this.graphSettings[combinedGraphName] = {
            viewer: viewerConfig,
            adapter: adapterConfig
          };
          localStorage.setItem('graphSettings', JSON.stringify(this.graphSettings));
        }
      }

    };
    this.loadLocalStorage();
    
    this.writeLocalStorage = function() {

    };
  }

  graphViewer = new GraphViewer(svg, width, height, adapterConfig, viewerConfig);

  createToolbox();
  createZoomUIWidget();
  createMenu();
  createColourList();
  makeDisplayInformationDiv();
  createOptionBox();

  $('#graphSize').on('change', function() {
    var size = $('#graphSize').find(":selected").val();
      graphViewer.loadGraphWithRandomStart(function(node) {
        if (node && node.errorCode) {
          arangoHelper.arangoError("Graph", "Sorry your graph seems to be empty");
        }
      }, size);
  });

  if (startNode) {
    if (typeof startNode === "string") {
      graphViewer.loadGraph(startNode);
    } else {
      graphViewer.loadGraphWithRandomStart(function(node) {
        if (node && node.errorCode) {
          arangoHelper.arangoError("Graph", "Sorry your graph seems to be empty");
        }
      });
    }
  }

  this.changeWidth = function(w) {
    graphViewer.changeWidth(w);
    var reducedW = w - 55;
    svg.attr("width", reducedW)
      .style("width", reducedW + "px");
  };

  //add events for writing/reading local storage (input fields)


  
}
