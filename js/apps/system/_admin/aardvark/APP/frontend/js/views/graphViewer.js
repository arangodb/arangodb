/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, _, frontendConfig, slicePath, icon, Joi, wheelnav, document, sigma, Backbone, templateEngine, $, window, JSONEditor */
(function () {
  'use strict';

  window.GraphViewer = Backbone.View.extend({
    el: '#content',

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    template: templateEngine.createTemplate('graphViewer2.ejs'),

    initialize: function (options) {
      var self = this;

      // aql preview only mode
      if (options.id) {
        // dynamically set id if available
        this.setElement(options.id);
        this.graphData = options.data;
        this.aqlMode = true;
      }

      // aql to graph viewer mode
      if (options.noDefinedGraph) {
        this.noDefinedGraph = options.noDefinedGraph;
        this.graphData = options.data;
      }

      this.name = options.name;
      this.userConfig = options.userConfig;
      this.documentStore = options.documentStore;

      if (this.name !== undefined) {
        this.collection.fetch({
          cache: false,
          success: function (data) {
            self.model = self.collection.findWhere({_key: options.name}).toJSON();
          }
        });
      }
    },

    colors: {
      hotaru: ['#364C4A', '#497C7F', '#92C5C0', '#858168', '#CCBCA5'],
      random1: ['#292F36', '#4ECDC4', '#F7FFF7', '#DD6363', '#FFE66D'],
      jans: ['rgba(166, 109, 161, 1)', 'rgba(64, 74, 83, 1)', 'rgba(90, 147, 189, 1)', 'rgba(153,63,0,1)', 'rgba(76,0,92,1)', 'rgba(25,25,25,1)', 'rgba(0,92,49,1)', 'rgba(43,206,72,1)', 'rgba(255,204,153,1)', 'rgba(128,128,128,1)', 'rgba(148,255,181,1)', 'rgba(143,124,0,1)', 'rgba(157,204,0,1)', 'rgba(194,0,136,1)', 'rgba(0,51,128,1)', 'rgba(255,164,5,1)', 'rgba(255,168,187,1)', 'rgba(66,102,0,1)', 'rgba(255,0,16,1)', 'rgba(94,241,242,1)', 'rgba(0,153,143,1)', 'rgba(224,255,102,1)', 'rgba(116,10,255,1)', 'rgba(153,0,0,1)', 'rgba(255,255,128,1)', 'rgba(255,255,0,1)', 'rgba(255,80,5,1)'],
      gv: [
        '#68BDF6',
        '#6DCE9E',
        '#FF756E',
        '#DE9BF9',
        '#FB95AF',
        '#FFD86E',
        '#A5ABB6'
      ]
    },

    activeNodes: [],
    selectedNodes: {},

    aqlMode: false,

    events: {
      'click #downloadPNG': 'downloadPNG',
      'click #loadFullGraph': 'loadFullGraphModal',
      'click #reloadGraph': 'reloadGraph',
      'click #settingsMenu': 'toggleSettings',
      'click #toggleForce': 'toggleLayout',
      'click #selectNodes': 'toggleLasso'
    },

    cursorX: 0,
    cursorY: 0,

    layouting: false,

    model: null,

    viewStates: {
      captureMode: false
    },

    graphConfig: null,
    graphSettings: null,

    downloadPNG: function () {
      var size = parseInt($('#graph-container').width(), 10);
      sigma.plugins.image(this.currentGraph, this.currentGraph.renderers[0], {
        download: true,
        size: size,
        clip: true,
        labels: true,
        background: 'white',
        zoom: false
      });
    },

    loadFullGraphModal: function () {
      var buttons = []; var tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          'load-full-graph-a',
          'Caution',
          'Really load full graph? If no limit is set, your result set could be too big.')
      );

      buttons.push(
        window.modalView.createSuccessButton('Load full graph', this.loadFullGraph.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Load full graph',
        buttons,
        tableContent
      );
    },

    loadFullGraph: function () {
      var self = this;
      var ajaxData = {};

      if (this.graphConfig) {
        ajaxData = _.clone(this.graphConfig);

        // remove not needed params
        delete ajaxData.layout;
        delete ajaxData.edgeType;
        delete ajaxData.renderer;
      }
      ajaxData.mode = 'all';

      $.ajax({
        type: 'GET',
        url: arangoHelper.databaseUrl('/_admin/aardvark/graph/' + encodeURIComponent(this.name)),
        contentType: 'application/json',
        data: ajaxData,
        success: function (data) {
          self.killCurrentGraph();
          self.renderGraph(data);
        },
        error: function (e) {
          arangoHelper.arangoError('Graph', 'Could not load full graph.');
        }
      });
      window.modalView.hide();
    },

    resize: function () {
      // adjust container widht + height
      $('#graph-container').width($('.centralContent').width());
      $('#graph-container').height($('.centralRow').height() - 155);
    },

    toggleSettings: function () {
      this.graphSettingsView.toggle();
    },

    render: function (toFocus) {
      this.$el.html(this.template.render({}));

      // render navigation
      $('#subNavigationBar .breadcrumb').html(
        'Graph: ' + this.name
      );

      this.resize();
      this.fetchGraph(toFocus);

      this.initFullscreen();
    },

    initFullscreen: function () {
      var self = this;

      if (window.App.initializedFullscreen === false || window.App.initializedFullscreen === undefined) {
        window.App.initializedFullscreen = true;
        this.isFullscreen = false;

        var exitHandler = function (a) {
          if (document.webkitIsFullScreen || document.mozFullScreen || document.msFullscreenElement !== null) {
            if (self.isFullscreen === false) {
              self.isFullscreen = true;

              // FULLSCREEN STYLING
              $('#toggleForce').css('bottom', '10px');
              $('#toggleForce').css('right', '10px');

              $('#objectCount').css('bottom', '10px');
              $('#objectCount').css('left', '10px');

              $('.nodeInfoDiv').css('top', '10px');
              $('.nodeInfoDiv').css('left', '10px');
            } else {
              self.isFullscreen = false;

              // NO FULLSCREEN STYLING
              $('#toggleForce').css('bottom', '40px');
              $('#toggleForce').css('right', '40px');

              $('#objectCount').css('bottom', '50px');
              $('#objectCount').css('left', '25px');

              $('.nodeInfoDiv').css('top', '');
              $('.nodeInfoDiv').css('left', '165px');
            }
          }
        };

        if (document.addEventListener) {
          document.addEventListener('webkitfullscreenchange', exitHandler, false);
          document.addEventListener('mozfullscreenchange', exitHandler, false);
          document.addEventListener('fullscreenchange', exitHandler, false);
          document.addEventListener('MSFullscreenChange', exitHandler, false);
        }
      }
    },

    renderAQLPreview: function (data) {
      this.$el.html(this.template.render({}));

      // remove not needed elements
      this.$el.find('.headerBar').remove();

      // set graph box height
      var height = $('.centralRow').height() - 250;
      this.$el.find('#graph-container').css('height', height);

      // render
      this.graphData.modified = this.parseData(this.graphData.original, this.graphData.graphInfo);

      var success = false;
      try {
        this.renderGraph(this.graphData.modified, null, true);
        success = true;
      } catch (ignore) {
      }

      return success;
    },

    renderAQL: function (data) {
      this.$el.html(this.template.render({}));

      // render navigation
      $('#subNavigationBar .breadcrumb').html(
        'AQL Graph'
      );
      $('#subNavigationBar .bottom').html('');
      $('.queries-menu').removeClass('active');

      this.resize();
      this.graphData.modified = this.parseData(this.graphData.original, this.graphData.graphInfo);
      this.renderGraph(this.graphData.modified, null, false);

      this.initFullscreen();

      // init & render graph settings view
      this.graphSettingsView = new window.GraphSettingsView({
        name: this.name,
        userConfig: undefined,
        saveCallback: undefined,
        noDefinedGraph: true
      });
      this.graphSettingsView.render();
    },

    killCurrentGraph: function () {
      if (this.currentGraph && this.currentGraph.renderers) {
        for (var i in this.currentGraph.renderers) {
          try {
            this.currentGraph.renderers[i].clear();
            this.currentGraph.kill(i);
          } catch (ignore) {
            // no need to cleanup
          }
        }
      }
    },

    rerenderAQL: function (layout, renderer) {
      this.killCurrentGraph();
      // TODO add WebGL features
      this.renderGraph(this.graphData.modified, null, false, layout, 'canvas');
      if ($('#g_nodeColorByCollection').val() === 'true') {
        this.switchNodeColorByCollection(true);
      } else {
        if ($('#g_nodeColor').is(':disabled')) {
          this.updateColors(true, true, null, null, true);
        } else {
          if (this.ncolor) {
            this.updateColors(true, true, this.ncolor, this.ecolor);
          } else {
            this.updateColors(true, true, '#2ecc71', '#2ecc71');
          }
        }
      }

      if ($('#g_edgeColorByCollection').val() === 'true') {
        this.switchEdgeColorByCollection(true);
      } else {
        if ($('#g_edgeColor').is(':disabled')) {
          this.updateColors(true, true, null, null, true);
        } else {
          if (this.ecolor) {
            this.updateColors(true, true, this.ncolor, this.ecolor);
          } else {
            this.updateColors(true, true, '#2ecc71', '#2ecc71');
          }
        }
      }
    },

    buildCollectionColors: function () {
      var self = this;

      if (!self.collectionColors) {
        self.collectionColors = {};
        var pos = 0;
        var tmpNodes = {};
        var tmpEdges = {};

        _.each(this.currentGraph.graph.nodes(), function (node) {
          tmpNodes[node.id] = undefined;
        });

        _.each(self.currentGraph.graph.edges(), function (edge) {
          tmpEdges[edge.id] = undefined;
        });

        _.each(tmpNodes, function (node, key) {
          if (self.collectionColors[key.split('/')[0]] === undefined) {
            self.collectionColors[key.split('/')[0]] = {color: self.colors.jans[pos]};
            pos++;
          }
        });

        pos = 0;
        _.each(tmpEdges, function (edge, key) {
          if (self.collectionColors[key.split('/')[0]] === undefined) {
            self.collectionColors[key.split('/')[0]] = {color: self.colors.jans[pos]};
            pos++;
          }
        });
      }
    },

    switchNodeColorByCollection: function (boolean, origin) {
      var self = this;
      self.buildCollectionColors();
      if (boolean) {
        self.currentGraph.graph.nodes().forEach(function (n) {
          n.color = self.collectionColors[n.id.split('/')[0]].color;
        });

        self.currentGraph.refresh();
      } else {
        if (origin) {
          this.updateColors(true, null, null, null, origin);
        } else {
          if (this.ncolor) {
            this.updateColors(true, null, this.ncolor, this.ecolor);
          } else {
            this.updateColors(true, null, '#2ecc71', '#2ecc71');
          }
        }
      }
    },

    switchEdgeColorByCollection: function (boolean, origin) {
      var self = this;
      self.buildCollectionColors();

      if (boolean) {
        self.currentGraph.graph.edges().forEach(function (n) {
          n.color = self.collectionColors[n.id.split('/')[0]].color;
        });

        self.currentGraph.refresh();
      } else {
        if (origin) {
          this.updateColors(true, null, null, null, origin);
        } else {
          if (this.ecolor) {
            this.updateColors(null, true, this.ncolor, this.ecolor);
          } else {
            this.updateColors(null, true, '#2ecc71', '#2ecc71');
          }
        }
      }
    },

    buildCollectionSizes: function () {
      var self = this;

      if (!self.nodeEdgesCount) {
        self.nodeEdgesCount = {};
        var handledEdges = {};

        _.each(this.currentGraph.graph.edges(), function (edge) {
          if (handledEdges[edge.id] === undefined) {
            handledEdges[edge.id] = true;

            if (self.nodeEdgesCount[edge.source] === undefined) {
              self.nodeEdgesCount[edge.source] = 1;
            } else {
              self.nodeEdgesCount[edge.source] += 1;
            }

            if (self.nodeEdgesCount[edge.target] === undefined) {
              self.nodeEdgesCount[edge.target] = 1;
            } else {
              self.nodeEdgesCount[edge.target] += 1;
            }
          }
        });
      }
    },

    switchNodeSizeByCollection: function (boolean) {
      var self = this;
      if (boolean) {
        self.buildCollectionSizes();
        self.currentGraph.graph.nodes().forEach(function (n) {
          n.size = self.nodeEdgesCount[n.id];
        });
      } else {
        self.currentGraph.graph.nodes().forEach(function (n) {
          n.size = 15;
        });
      }
      self.currentGraph.refresh();
    },

    switchEdgeType: function (edgeType) {
      var data = {
        nodes: this.currentGraph.graph.nodes(),
        edges: this.currentGraph.graph.edges(),
        settings: {}
      };

      this.killCurrentGraph();
      this.renderGraph(data, null, false, null, null, edgeType);
    },

    switchLayout: function (layout) {
      var data = {
        nodes: this.currentGraph.graph.nodes(),
        edges: this.currentGraph.graph.edges(),
        settings: {}
      };

      this.killCurrentGraph();
      this.renderGraph(data, null, false, layout);

      if ($('#g_nodeColorByCollection').val() === 'true') {
        this.switchNodeColorByCollection(true);
      }
      if ($('#g_edgeColorByCollection').val() === 'true') {
        this.switchEdgeColorByCollection(true);
      } else {
        this.switchEdgeColorByCollection(false);
      }
    },

    parseData: function (data, type) {
      var vertices = {}; var edges = {};
      var color = '#2ecc71';

      var returnObj = {
        nodes: [],
        edges: [],
        settings: {}
      };

      if (this.ncolor) {
        color = this.ncolor;
      }

      if (type === 'object') {
        _.each(data, function (obj) {
          if (obj.edges && obj.vertices) {
            _.each(obj.edges, function (edge) {
              if (edge !== null) {
                edges[edge._id] = {
                  id: edge._id,
                  source: edge._from,
                  // label: edge._key,
                  color: '#cccccc',
                  target: edge._to
                };
              }
            });

            _.each(obj.vertices, function (node) {
              if (node !== null) {
                vertices[node._id] = {
                  id: node._id,
                  label: node._key,
                  size: 0.3,
                  color: color,
                  x: Math.random(),
                  y: Math.random()
                };
              }
            });
          }
        });

        var nodeIds = [];
        _.each(vertices, function (node) {
          returnObj.nodes.push(node);
          nodeIds.push(node.id);
        });

        _.each(edges, function (edge) {
          if (nodeIds.includes(edge.source) && nodeIds.includes(edge.target)) {
            returnObj.edges.push(edge);
          }
          /* how to handle not correct data?
          else {
            console.log('target to from is missing');
          }
          */
        });
      } else if (type === 'array') {
        var edgeObj = {};

        _.each(data, function (edge) {
          if (edge) {
            vertices[edge._from] = null;
            vertices[edge._to] = null;

            if (edge._id) {
              edgeObj[edge._id] = {
                id: edge._id,
                source: edge._from,
                color: '#cccccc',
                target: edge._to
              };
            }
          }
        });

        _.each(vertices, function (val, key) {
          returnObj.nodes.push({
            id: key,
            label: key,
            size: 0.3,
            color: color,
            x: Math.random(),
            y: Math.random()
          });
        });
        _.each(edgeObj, function (edge) {
          returnObj.edges.push(edge);
        });
      }

      return returnObj;
    },

    rerender: function () {
      this.fetchGraph();
    },

    fetchGraph: function (toFocus) {
      var self = this;
      // arangoHelper.buildGraphSubNav(self.name, 'Content');
      $(this.el).append(
        '<div id="calculatingGraph" style="position: absolute; left: 25px; top: 130px;">' +
          '<i class="fa fa-circle-o-notch fa-spin" style="margin-right: 10px;"></i>' +
            '<span id="calcText">Fetching graph data. Please wait ... </span></br></br></br>' +
              '<span style="font-weight: 100; opacity: 0.6; font-size: 9pt;">If it`s taking too much time to draw the graph, please navigate to: ' +
                '<a style="font-weight: 500" href="' + window.location.href + '/graphs">Graphs View</a></br>Click the settings icon and reset the display settings.' +
                  'It is possible that the graph is too big to be handled by the browser.</span></div>'
      );

      var continueFetchGraph = function () {
        var ajaxData = {};
        if (self.graphConfig) {
          ajaxData = _.clone(self.graphConfig);

          // remove not needed params - client only
          delete ajaxData.layout;
          delete ajaxData.edgeType;
          delete ajaxData.renderer;
        }

        if (self.tmpStartNode) {
          if (self.graphConfig) {
            if (self.graphConfig.nodeStart.length === 0) {
              ajaxData.nodeStart = self.tmpStartNode;
            }
          } else {
            ajaxData.nodeStart = self.tmpStartNode;
          }
        }

        self.setupSigma();

        self.fetchStarted = new Date();
        $.ajax({
          type: 'GET',
          url: arangoHelper.databaseUrl('/_admin/aardvark/graph/' + encodeURIComponent(self.name)),
          contentType: 'application/json',
          data: ajaxData,
          success: function (data) {
            if (data.empty === true) {
              self.renderGraph(data, toFocus);
            } else {
              if (data.settings) {
                if (data.settings.startVertex && self.graphConfig.startNode === undefined) {
                  if (self.tmpStartNode === undefined) {
                    self.tmpStartNode = data.settings.startVertex._id;
                  }
                }
              }

              self.fetchFinished = new Date();
              self.calcStart = self.fetchFinished;
              $('#calcText').html('Server response took ' + Math.abs(self.fetchFinished.getTime() - self.fetchStarted.getTime()) + ' ms. Initializing graph engine. Please wait ... ');
              // arangoHelper.buildGraphSubNav(self.name, 'Content');
              window.setTimeout(function () {
                self.renderGraph(data, toFocus);
              }, 50);
            }
          },
          error: function (e) {
            try {
              var message;
              if (e.responseJSON.exception) {
                message = e.responseJSON.exception;
                var found = e.responseJSON.exception.search('1205');
                if (found !== -1) {
                  var string = 'Starting point: <span style="font-weight: 400">' + self.graphConfig.nodeStart + '</span> is invalid';
                  $('#calculatingGraph').html(
                    '<div style="font-weight: 300; font-size: 10.5pt"><span style="font-weight: 400">Stopped. </span></br></br>' +
                    string +
                    '. Please <a style="color: #3498db" href="' + window.location.href +
                    '/settings">choose a different start node.</a></div>'
                  );
                } else {
                  $('#calculatingGraph').html('Failed to fetch graph information.');
                }
              } else {
                message = e.responseJSON.errorMessage;
                $('#calculatingGraph').html('Failed to fetch graph information: ' + e.responseJSON.errorMessage);
              }
              arangoHelper.arangoError('Graph', message);
            } catch (ignore) {}
          }
        });
      };

      if (self.graphConfig === undefined || self.graphConfig === null) {
        self.userConfig.fetch({
          success: function (data) {
            var combinedName = frontendConfig.db + '_' + self.name;
            try {
              self.graphConfig = data.toJSON().graphs[combinedName];
              self.getGraphSettings(continueFetchGraph);

              if (self.graphConfig === undefined || self.graphConfig === null) {
                self.graphSettingsView = new window.GraphSettingsView({
                  name: self.name,
                  userConfig: self.userConfig,
                  saveCallback: self.render
                });
                self.graphSettingsView.setDefaults(true, true);
              } else {
                // init settings view
                if (self.graphSettingsView) {
                  self.graphSettingsView.remove();
                }
                self.graphSettingsView = new window.GraphSettingsView({
                  name: self.name,
                  userConfig: self.userConfig,
                  saveCallback: self.render
                });
                self.graphSettingsView.render();
              }
            } catch (ignore) {
              // continue without config
              self.getGraphSettings(continueFetchGraph);
            }
          }
        });
      } else {
        this.getGraphSettings(continueFetchGraph);
      }
    },

    setupSigma: function () {
      if (this.graphConfig) {
        if (this.graphConfig.edgeLabel) {
          // Initialize package:
          sigma.utils.pkg('sigma.settings');

          var settings = {
            defaultEdgeLabelColor: '#000',
            defaultEdgeLabelActiveColor: '#000',
            defaultEdgeLabelSize: 12,
            edgeLabelSize: 'fixed',
            edgeLabelThreshold: 1,
            edgeLabelSizePowRatio: 1
          };

          // Export the previously designed settings:
          sigma.settings = sigma.utils.extend(sigma.settings || {}, settings);

          // Override default settings:
          sigma.settings.drawEdgeLabels = true;
          sigma.settings.clone = true;
        }
      }
    },

    contextState: {
      createEdge: false,
      _from: false,
      _to: false,
      fromX: false,
      fromY: false
    },

    removeOldContextMenu: function () {
      $('#nodeContextMenu').remove();
    },

    clearOldContextMenu: function (states) {
      var self = this;

      // clear dom
      $('#nodeContextMenu').remove();
      var string = '<div id="nodeContextMenu" class="nodeContextMenu animated zoomIn"></div>';
      $('#graph-container').append(string);

      // clear state
      if (states) {
        _.each(this.contextState, function (val, key) {
          self.contextState[key] = false;
        });
      }

      // clear events
      var c = document.getElementsByClassName('sigma-mouse')[0];
      c.removeEventListener('mousemove', self.drawLine.bind(this), false);
    },

    trackCursorPosition: function (e) {
      this.cursorX = e.x;
      this.cursorY = e.y;
    },

    deleteNode: function (e, id) {
      var self = this;
      var documentKey;
      var collectionId;
      var documentId;

      if (id) {
        documentKey = id;
      } else {
        documentKey = $('#delete-node-attr-id').text();
      }
      collectionId = documentKey.split('/')[0];
      documentId = documentKey.split('/')[1];

      var url = arangoHelper.databaseUrl(
        '/_api/gharial/' + encodeURIComponent(self.name) + '/vertex/' + encodeURIComponent(documentKey.split('/')[0]) + '/' + encodeURIComponent(documentKey.split('/')[1])
      );

      if ($('#delete-node-edges-attr').val() === 'yes') {
        $.ajax({
          cache: false,
          type: 'DELETE',
          contentType: 'application/json',
          url: url,
          success: function (data) {
            self.currentGraph.graph.dropNode(documentKey);
            self.currentGraph.refresh();
          },
          error: function () {
            arangoHelper.arangoError('Graph', 'Could not delete node.');
          }
        });
      } else {
        var callback = function (error) {
          if (!error) {
            self.currentGraph.graph.dropNode(documentKey);

            // rerender graph
            self.currentGraph.refresh();
          } else {
            arangoHelper.arangoError('Graph', 'Could not delete node.');
          }
        };

        this.documentStore.deleteDocument(collectionId, documentId, callback);
      }
      window.modalView.hide();
    },

    deleteNodes: function () {
      var self = this;

      try {
        var arr = JSON.parse($('#delete-nodes-arr-id').text());
        _.each(arr, function (id) {
          self.deleteNode(null, id);
        });
      } catch (ignore) {
      }
    },

    deleteNodesModal: function () {
      var nodeIds = [];

      _.each(this.selectedNodes, function (id) {
        nodeIds.push(id);
      });

      if (nodeIds.length === 0) {
        arangoHelper.arangoNotification('Graph', 'No nodes selected.');
        return;
      }

      var buttons = []; var tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry('delete-nodes-arr-id', 'Really delete nodes', JSON.stringify(nodeIds))
      );

      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteNodes.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Delete nodes',
        buttons,
        tableContent
      );
    },

    deleteNodeModal: function (nodeId) {
      var buttons = []; var tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry('delete-node-attr-id', 'Really delete node', nodeId)
      );

      if (!this.noDefinedGraph) {
        tableContent.push(
          window.modalView.createSelectEntry(
            'delete-node-edges-attr',
            'Also delete edges?',
            undefined,
            undefined,
            [
              {
                value: 'yes',
                label: 'Yes'
              },
              {
                value: 'no',
                label: 'No'
              }
            ]
          )
        );
      }

      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteNode.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Delete node',
        buttons,
        tableContent
      );
    },

    addNode: function () {
      var self = this;

      var collection = $('.modal-body #new-node-collection-attr').val();
      var key = $('.modal-body #new-node-key-attr').last().val();

      var callback = function (error, id, msg) {
        if (error) {
          arangoHelper.arangoError('Could not create node', msg);
        } else {
          $('#emptyGraph').remove();
          self.currentGraph.graph.addNode({
            id: id,
            label: id.split('/')[1] || '',
            size: self.graphConfig.nodeSize || 15,
            color: self.graphConfig.nodeColor || self.ncolor || '#2ecc71',
            originalColor: self.graphConfig.nodeColor || self.ncolor || '#2ecc71',
            x: self.addNodeX + self.currentGraph.camera.x,
            y: self.addNodeY + self.currentGraph.camera.y
          });

          window.modalView.hide();
          // rerender graph
          self.currentGraph.refresh();

          // move camera to added node
          self.cameraToNode(self.currentGraph.graph.nodes(id));
        }
      };

      var body = {};
      try {
        body = this.editor.get();
      } catch (x) {
        arangoHelper.arangoError("failed to parse JSON document", x.message);
        return;
      }
      if (key !== '' && key !== undefined) {
        body._key = key;
      }
      if (this.graphSettings.isSmart) {
        var smartAttribute = $('#new-smart-key-attr').val();
        if (smartAttribute !== '' && smartAttribute !== undefined) {
          body[this.graphSettings.smartGraphAttribute] = smartAttribute;
        } else {
          body[this.graphSettings.smartGraphAttribute] = null;
        }
      }

      this.collection.createNode(self.name, collection, body, callback);
    },

    deleteEdgeModal: function (edgeId) {
      var buttons = []; var tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry('delete-edge-attr-id', 'Really delete edge', edgeId)
      );

      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteEdge.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Delete edge',
        buttons,
        tableContent
      );
    },

    deleteEdge: function () {
      var self = this;
      var documentKey = $('#delete-edge-attr-id').text();
      var collectionId = documentKey.split('/')[0];
      var documentId = documentKey.split('/')[1];

      var callback = function (error) {
        if (!error) {
          self.currentGraph.graph.dropEdge(documentKey);

          // rerender graph
          self.currentGraph.refresh();
        } else {
          arangoHelper.arangoError('Graph', 'Could not delete edge.');
        }
      };

      this.documentStore.deleteDocument(collectionId, documentId, callback);
      window.modalView.hide();
    },

    addNodeModal: function () {
      if (this.graphSettings.vertexCollections !== 0) {
        var buttons = []; var tableContent = []; var collections = [];

        _.each(this.graphSettings.vertexCollections, function (val) {
          collections.push({
            label: val.name,
            value: val.name
          });
        });

        tableContent.push(
          window.modalView.createTextEntry(
            'new-node-key-attr',
            '_key',
            undefined,
            'The node\'s unique key (optional attribute, leave empty for autogenerated key)',
            'is optional: leave empty for autogenerated key',
            false,
            [
              {
                rule: Joi.string().allow('').optional(),
                msg: ''
              }
            ]
          )
        );

        if (this.graphSettings.isSmart) {
          tableContent.push(
            window.modalView.createTextEntry(
              'new-smart-key-attr',
              this.graphSettings.smartGraphAttribute + '*',
              undefined,
              'The attribute value that is used to shard the vertices of a SmartGraph. \n' +
              'Every vertex in this SmartGraph has to have this attribute. \n' +
              'Cannot be modified later.',
              'Cannot be modified later.',
              false,
              [
                {
                  rule: Joi.string().allow('').optional(),
                  msg: ''
                }
              ]
            )
          );
        }

        tableContent.push(
          window.modalView.createSelectEntry(
            'new-node-collection-attr',
            'Collection',
            undefined,
            'Please select the target collection for the new node.',
            collections
          )
        );

        tableContent.push(window.modalView.createJsonEditor());

        buttons.push(
          window.modalView.createSuccessButton('Create', this.addNode.bind(this))
        );

        window.modalView.show(
          'modalTable.ejs',
          'Create node',
          buttons,
          tableContent
        );
        var container = document.getElementById('jsoneditor');
        this.resize();
        var options = {
          onChange: function () {
          },
          onModeChange: function (newMode) {
            void (newMode);
          },
          search: true,
          mode: 'code',
          modes: ['tree', 'code'],
          ace: window.ace
        };
        this.editor = new JSONEditor(container, options);
      } else {
        arangoHelper.arangoError('Graph', 'No valid vertex collections found.');
      }
    },

    addEdge: function () {
      var self = this;
      var from = self.contextState._from;
      var to = self.contextState._to;

      var collection;
      if ($('.modal-body #new-edge-collection-attr').val() === '') {
        collection = $('.modal-body #new-edge-collection-attr').text();
      } else {
        collection = $('.modal-body #new-edge-collection-attr').val();
      }
      var key = $('.modal-body #new-edge-key-attr').last().val();

      var callback = function (error, id, msg) {
        if (!error) {
          var edge;
          try {
            edge = this.editor.get();
          } catch (x) {
            arangoHelper.arangoError("failed to parse JSON document", x.message);
            return;
          }
          try {
            edge.source = from;
          } catch (x) {
            edge = {};
            edge.source = from;
          }
          edge.target = to;
          edge.id = id;
          edge.color = self.graphConfig.edgeColor || self.ecolor;

          if (self.graphConfig.edgeEditable === 'true') {
            edge.size = 1;
          }
          self.currentGraph.graph.addEdge(edge);

          // rerender graph
          if (self.graphConfig) {
            if (self.graphConfig.edgeType === 'curve') {
              sigma.canvas.edges.autoCurve(self.currentGraph);
            }
          }
          self.currentGraph.refresh();
        } else {
          arangoHelper.arangoError('Could not create edge', msg);
        }

        // then clear states
        self.clearOldContextMenu(true);
        window.modalView.hide();
      }.bind(this);

      var body;
      try {
        body = this.editor.get();
      } catch (x) {
        arangoHelper.arangoError("failed to parse JSON document", x.message);
        return;
      }

      try {
        body._from = from;
      } catch (x) {
        body = {};
        body._from = from;
      }

      body._to = to;
      if (key !== '' && key !== undefined) {
        body._key = key;
      }
      this.collection.createEdge(self.name, collection, body, callback);
    },

    addEdgeModal: function (edgeDefinitions) {
      if (edgeDefinitions !== 0) {
        var buttons = []; var tableContent = [];

        tableContent.push(
          window.modalView.createTextEntry(
            'new-edge-key-attr',
            '_key',
            undefined,
            'The edge\'s unique key (optional attribute, leave empty for autogenerated key)',
            'is optional: leave empty for autogenerated key',
            false,
            [
              {
                rule: Joi.string().allow('').optional(),
                msg: ''
              }
            ]
          )
        );

        if (edgeDefinitions.length > 1) {
          var collections = [];

          _.each(edgeDefinitions, function (val) {
            collections.push({
              label: val,
              value: val
            });
          });

          tableContent.push(
            window.modalView.createSelectEntry(
              'new-edge-collection-attr',
              'Edge collection',
              undefined,
              'Please select the target collection for the new edge.',
              collections
            )
          );
        } else {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'new-edge-collection-attr',
              'Edge collection',
              edgeDefinitions[0],
              'The edge collection to be used.'
            )
          );
        }

        tableContent.push(window.modalView.createJsonEditor());

        buttons.push(
          window.modalView.createSuccessButton('Create', this.addEdge.bind(this))
        );

        window.modalView.show(
          'modalTable.ejs',
          'Create edge',
          buttons,
          tableContent
        );
        var container = document.getElementById('jsoneditor');
        this.resize();
        var options = {
          onChange: function () {
          },
          onModeChange: function (newMode) {
            void (newMode);
          },
          search: true,
          mode: 'code',
          modes: ['tree', 'code'],
          ace: window.ace
        };
        this.editor = new JSONEditor(container, options);
      } else {
        arangoHelper.arangoError('Graph', 'No valid edge definitions found.');
      }
    },

    updateColors: function (nodes, edges, ncolor, ecolor, origin) {
      var combinedName = frontendConfig.db + '_' + this.name;
      var self = this;

      if (ncolor) {
        self.ncolor = ncolor;
      }
      if (ecolor) {
        self.ecolor = ecolor;
      }

      this.userConfig.fetch({
        success: function (data) {
          if (nodes === true) {
            self.graphConfig = data.toJSON().graphs[combinedName];
            try {
              self.currentGraph.graph.nodes().forEach(function (n) {
                if (origin) {
                  n.color = n.sortColor;
                } else {
                  n.color = ncolor;
                }
              });
            } catch (e) {
              self.graphNotInitialized = true;
              self.tmpGraphArray = [nodes, edges, ncolor, ecolor];
            }
          }

          if (edges === true) {
            try {
              self.currentGraph.graph.edges().forEach(function (e) {
                if (origin) {
                  e.color = e.sortColor;
                } else {
                  e.color = ecolor;
                }
              });
            } catch (ignore) {
              self.graphNotInitialized = true;
              self.tmpGraphArray = [nodes, edges, ncolor, ecolor];
            }
          }

          if (self.currentGraph) {
            self.currentGraph.refresh();
          }
        }
      });
    },

    nodesContextMenuCheck: function (e) {
      this.nodesContextEventState = e;
      this.openNodesDate = new Date();
    },

    // right click background context menu
    createContextMenu: function (e) {
      var self = this;
      var x = self.cursorX - 50;
      var y = self.cursorY - 50;
      this.clearOldContextMenu();

      var generateMenu = function (e) {
        var Wheelnav = wheelnav;

        var wheel = new Wheelnav('nodeContextMenu');
        wheel.maxPercent = 1.0;
        wheel.wheelRadius = 50;
        wheel.clockwise = false;
        wheel.colors = self.colors.hotaru;
        wheel.multiSelect = true;
        wheel.clickModeRotate = false;
        wheel.slicePathFunction = slicePath().DonutSlice;
        wheel.createWheel([icon.plus, icon.arrowleft2]);

        // add menu events
        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;

        // function 0: add node
        wheel.navItems[0].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.addNodeModal();
          self.removeOldContextMenu();
          self.removeHelp();
        };

        // function 1: exit
        wheel.navItems[1].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.removeOldContextMenu();
        };

        var descriptions = [
          'Add new node.',
          'Close menu.'
        ];

        // hover functions
        _.each(descriptions, function (val, key) {
          wheel.navItems[key].navTitle.mouseover(function () { self.drawHelp(val); });
          wheel.navItems[key].navTitle.mouseout(function () { self.removeHelp(); });
        });

        // deselect active default entry
        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;
      };

      $('#nodeContextMenu').css('position', 'fixed');
      $('#nodeContextMenu').css('left', x);
      $('#nodeContextMenu').css('top', y);
      $('#nodeContextMenu').width(100);
      $('#nodeContextMenu').height(100);

      generateMenu(e);
    },

    // click edge context menu
    createEdgeContextMenu: function (edgeId, e) {
      var self = this;
      var x = this.cursorX - 165;
      var y = this.cursorY - 120;

      this.clearOldContextMenu();

      var generateMenu = function (e, edgeId) {
        var hotaru = ['#364C4A', '#497C7F', '#92C5C0', '#858168', '#CCBCA5'];

        var Wheelnav = wheelnav;

        var wheel = new Wheelnav('nodeContextMenu');
        wheel.maxPercent = 1.0;
        wheel.wheelRadius = 50;
        wheel.clockwise = false;
        wheel.colors = hotaru;
        wheel.multiSelect = true;
        wheel.clickModeRotate = false;
        wheel.slicePathFunction = slicePath().DonutSlice;
        wheel.createWheel([
          'imgsrc:img/gv_edit.png',
          'imgsrc:img/gv_trash.png'
        ]);

        // add menu events
        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;

        // function 0: edit the edge
        wheel.navItems[0].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.editEdge(edgeId);
          self.removeHelp();
        };

        // function 1: delete the edge
        wheel.navItems[1].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.deleteEdgeModal(edgeId);
          self.removeHelp();
        };

        var descriptions = [
          'Edit the edge.',
          'Delete the edge.'
        ];

        // hover functions
        _.each(descriptions, function (val, key) {
          wheel.navItems[key].navTitle.mouseover(function () { self.drawHelp(val); });
          wheel.navItems[key].navTitle.mouseout(function () { self.removeHelp(); });
        });

        // deselect active default entry
        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;
      };

      $('#nodeContextMenu').css('left', x + 115);
      $('#nodeContextMenu').css('top', y + 72);
      $('#nodeContextMenu').width(100);
      $('#nodeContextMenu').height(100);

      generateMenu(e, edgeId);
    },

    // click node context menu
    createNodeContextMenu: function (nodeId, e) {
      var self = this;
      var x; var y; var size;

      // case canvas
      _.each(e.data.node, function (val, key) {
        if (key.substr(0, 8) === 'renderer' && key.charAt(key.length - 1) === 'x') {
          x = val;
        }
        if (key.substr(0, 8) === 'renderer' && key.charAt(key.length - 1) === 'y') {
          y = val;
        }
        if (key.substr(0, 8) === 'renderer' && key.charAt(key.length - 1) === 'e') {
          size = val;
        }
      });

      if (x === undefined && y === undefined) {
        // case webgl
        _.each(e.data.node, function (val, key) {
          if (key.substr(0, 8) === 'read_cam' && key.charAt(key.length - 1) === 'x') {
            x = val + $('#graph-container').width() / 2;
          }
          if (key.substr(0, 8) === 'read_cam' && key.charAt(key.length - 1) === 'y') {
            y = val + $('#graph-container').height() / 2;
          }
        });
      }

      var radius = size * 2.5;

      if (radius < 75) {
        radius = 75;
      }

      this.clearOldContextMenu();
      var generateMenu = function (e, nodeId) {
        var hotaru = ['#364C4A', '#497C7F', '#92C5C0', '#858168', '#CCBCA5'];

        var Wheelnav = wheelnav;

        var wheel = new Wheelnav('nodeContextMenu');
        wheel.maxPercent = 1.0;
        wheel.wheelRadius = radius;
        wheel.clockwise = false;
        wheel.colors = hotaru;
        wheel.multiSelect = false;
        wheel.clickModeRotate = false;
        wheel.sliceHoverAttr = {stroke: '#fff', 'stroke-width': 2};
        wheel.slicePathFunction = slicePath().DonutSlice;

        if (!self.noDefinedGraph) {
          wheel.createWheel([
            'imgsrc:img/gv_edit.png',
            'imgsrc:img/gv_trash.png',
            'imgsrc:img/gv_flag.png',
            'imgsrc:img/gv_link.png',
            'imgsrc:img/gv_expand.png'
          ]);
        } else {
          wheel.createWheel([
            'imgsrc:img/gv_edit.png',
            'imgsrc:img/gv_trash.png'
          ]);
        }

        $('#nodeContextMenu').addClass('animated bounceIn');

        window.setTimeout(function () {
          // add menu events

          // function 0: edit
          wheel.navItems[0].navigateFunction = function (e) {
            self.clearOldContextMenu();
            self.editNode(nodeId);
            self.removeHelp();
          };

          // function 1: delete
          wheel.navItems[1].navigateFunction = function (e) {
            self.clearOldContextMenu();
            self.deleteNodeModal(nodeId);
            self.removeHelp();
          };

          if (!self.noDefinedGraph) {
            // function 2: mark as start node
            wheel.navItems[2].navigateFunction = function (e) {
              self.clearOldContextMenu();
              self.setStartNode(nodeId);
              self.removeHelp();
            };

            // function 3: create edge
            wheel.navItems[3].navigateFunction = function (e) {
              self.contextState.createEdge = true;
              self.contextState._from = nodeId;
              self.contextState.fromX = x;
              self.contextState.fromY = y;

              var c = document.getElementsByClassName('sigma-mouse')[0];
              self.drawHelp('Now click destination node, or click background to cancel.');
              c.addEventListener('mousemove', self.drawLine.bind(this), false);

              self.clearOldContextMenu();
              self.removeHelp();
            };

            // function 4: mark as start node
            wheel.navItems[4].navigateFunction = function (e) {
              self.clearOldContextMenu();
              self.expandNode(nodeId);
              self.removeHelp();
            };
          }

          // add menu hover functions

          var descriptions = [
            'Edit the node.',
            'Delete the node.'
          ];

          if (!self.noDefinedGraph) {
            descriptions.push('Set as startnode.');
            descriptions.push('Draw edge.');
            descriptions.push('Expand the node.');
          }

          // hover functions
          _.each(descriptions, function (val, key) {
            wheel.navItems[key].navTitle.mouseover(function () { self.drawHelp(val); });
            wheel.navItems[key].navTitle.mouseout(function () { self.removeHelp(); });
          });

          // deselect active default entry
          wheel.navItems[0].selected = false;
          wheel.navItems[0].hovered = false;
        }, 300);
      };

      var offset = $('#graph-container').offset();
      $('#nodeContextMenu').width(radius * 2);
      $('#nodeContextMenu').height(radius * 2);
      // $('#nodeContextMenu').css('left', e.data.captor.clientX - radius);
      // $('#nodeContextMenu').css('top', e.data.captor.clientY - radius);
      // $('#nodeContextMenu').css('left', x + 150 + 15 - radius);
      // $('#nodeContextMenu').css('top', y + 60 + 42 + 15 - radius);
      $('#nodeContextMenu').css('left', x + offset.left - radius);
      $('#nodeContextMenu').css('top', y + offset.top - radius);

      generateMenu(e, nodeId);
    },

    drawHelp: function (val) {
      if (document.getElementById('helpTooltip') === null) {
        $(this.el).append('<div id="helpTooltip" class="helpTooltip"><span>' + val + '</span></div>');
      } else {
        $('#helpTooltip span').text(val);
      }

      $('#helpTooltip').show();
    },

    removeHelp: function () {
      $('#helpTooltip').remove();
    },

    clearMouseCanvas: function () {
      var c = document.getElementsByClassName('sigma-mouse')[0];
      var ctx = c.getContext('2d');
      ctx.clearRect(0, 0, $(c).width(), $(c).height());
    },

    expandNode: function (id) {
      var self = this;
      var ajaxData = {};

      if (this.graphConfig) {
        ajaxData = _.clone(this.graphConfig);

        // remove not needed params
        delete ajaxData.layout;
        delete ajaxData.edgeType;
        delete ajaxData.renderer;
      }

      ajaxData.query = 'FOR v, e, p IN 1..1 ANY ' + JSON.stringify(id) + ' GRAPH ' + JSON.stringify(self.name) + ' RETURN p';

      $.ajax({
        type: 'GET',
        url: arangoHelper.databaseUrl('/_admin/aardvark/graph/' + encodeURIComponent(this.name)),
        contentType: 'application/json',
        data: ajaxData,
        success: function (data) {
          self.checkExpand(data, id);
        },
        error: function (e) {
          arangoHelper.arangoError('Graph', 'Could not expand node: ' + id + '.');
        }
      });

      self.removeHelp();
    },

    checkExpand: function (data, origin) {
      var self = this;
      var newNodes = data.nodes;
      var newEdges = data.edges;
      var existingNodes = this.currentGraph.graph.nodes();

      var found;
      var newNodeCounter = 0;
      var newEdgeCounter = 0;

      _.each(newNodes, function (newNode) {
        found = false;
        _.each(existingNodes, function (existingNode) {
          if (found === false) {
            if (newNode.id === existingNode.id) {
              if (existingNode.id === origin) {
                if (existingNode.label.indexOf(' (expanded)') === -1) {
                  existingNode.label = existingNode.label + ' (expanded)';
                }
              }
              found = true;
            } else {
              found = false;
            }
          }
        });

        if (found === false) {
          newNode.originalColor = newNode.color;
          self.currentGraph.graph.addNode(newNode);
          newNodeCounter++;
        }
      });

      _.each(newEdges, function (edge) {
        if (self.currentGraph.graph.edges(edge.id) === undefined) {
          edge.originalColor = edge.color;
          self.currentGraph.graph.addEdge(edge);
          newEdgeCounter++;
        }
      });

      $('#nodesCount').text(parseInt($('#nodesCount').text(), 10) + newNodeCounter);
      $('#edgesCount').text(parseInt($('#edgesCount').text(), 10) + newEdgeCounter);

      // rerender graph
      if (newNodeCounter > 0 || newEdgeCounter > 0) {
        if (self.algorithm === 'force') {
          self.startLayout(true, origin);
        } else if (self.algorithm === 'fruchtermann') {
          sigma.layouts.fruchtermanReingold.start(self.currentGraph);
          self.currentGraph.refresh();
          self.cameraToNode(origin, 1000);
        } else if (self.algorithm === 'noverlap') {
          self.startLayout(true, origin); // TODO: tmp bugfix, rerender with noverlap currently not possible
          // self.currentGraph.startNoverlap();
        }
      }
    },

    cameraToNode: function (node, timeout) {
      var self = this;

      if (typeof node === 'string') {
        node = self.currentGraph.graph.nodes(node);
      }

      var animateFunc = function (node) {
        sigma.misc.animation.camera(self.currentGraph.camera, {
          x: node.x,
          y: node.y
        }, {
          duration: 1000
        });
      };

      if (timeout) {
        window.setTimeout(function () {
          animateFunc(node);
        }, timeout);
      } else {
        animateFunc(node);
      }
    },

    drawLine: function (e) {
      var context = window.App.graphViewer.contextState;

      if (context.createEdge) {
        var fromX = context.fromX;
        var fromY = context.fromY;
        var toX = e.offsetX;
        var toY = e.offsetY;

        var c = document.getElementsByClassName('sigma-mouse')[0];
        var ctx = c.getContext('2d');
        ctx.clearRect(0, 0, $(c).width(), $(c).height());
        ctx.beginPath();
        ctx.moveTo(fromX, fromY);
        ctx.lineTo(toX, toY);
        ctx.strokeStyle = this.newEdgeColor;
        ctx.stroke();
      }
    },

    getGraphSettings: function (callback) {
      var self = this;

      this.userConfig.fetch({
        success: function (data) {
          var combinedName = frontendConfig.db + '_' + self.name;
          self.graphConfig = data.toJSON().graphs[combinedName];

          // init settings view
          if (self.graphSettingsView) {
            self.graphSettingsView.remove();
          }
          self.graphSettingsView = new window.GraphSettingsView({
            name: self.name,
            userConfig: self.userConfig,
            saveCallback: self.render
          });

          var continueFunction = function () {
            self.graphSettingsView.render();

            if (callback) {
              callback(self.graphConfig);
            }
          };

          if (self.graphConfig === undefined) {
            self.graphSettingsView.setDefaults(true, true);
            self.userConfig.fetch({
              success: function (data) {
                self.graphConfig = data.toJSON().graphs[combinedName];
                continueFunction();
              }
            });
          } else {
            continueFunction();
          }
        }
      });
    },

    setStartNode: function (id) {
      this.graphConfig.nodeStart = id;
      this.graphSettingsView.saveGraphSettings(undefined, undefined, id);
    },

    editNode: function (id) {
      var callback = function (data) {
        this.updateNodeLabel(data);
      }.bind(this);
      arangoHelper.openDocEditor(id, 'doc', callback);
    },

    updateNodeLabel: function (data) {
      var id = data[0]._id;

      if (this.graphConfig.nodeLabel) {
        var oldLabel = this.currentGraph.graph.nodes(id).label;
        if (oldLabel !== data[0][this.graphConfig.nodeLabel]) {
          var newLabel = data[0]['new'][this.graphConfig.nodeLabel];
          if (typeof newLabel === 'string') {
            this.currentGraph.graph.nodes(id).label = newLabel;
          } else {
            this.currentGraph.graph.nodes(id).label = JSON.stringify(newLabel);
          }
          this.currentGraph.refresh({ skipIndexation: true });
        }
      }
    },

    editEdge: function (id) {
      var callback = function (data) {
        this.updateEdgeLabel(data);
      }.bind(this);
      arangoHelper.openDocEditor(id, 'edge', callback);
    },

    updateEdgeLabel: function (data) {
      var id = data[0]._id;

      if (this.graphConfig.edgeLabel) {
        var oldLabel = this.currentGraph.graph.edges(id).label;
        if (oldLabel !== data[0][this.graphConfig.edgeLabel]) {
          var newLabel = data[0]['new'][this.graphConfig.edgeLabel];
          if (typeof newLabel === 'string') {
            this.currentGraph.graph.edges(id).label = newLabel;
          } else {
            this.currentGraph.graph.edges(id).label = JSON.stringify(newLabel);
          }
          this.currentGraph.refresh({ skipIndexation: true });
        }
      }
    },

    reloadGraph: function () {
      Backbone.history.loadUrl(Backbone.history.fragment);
    },

    getEdgeDefinitionCollections: function (fromCollection, toCollection) {
      var array = [];

      _.each(this.model.edgeDefinitions, function (edgeDefinition) {
        _.each(edgeDefinition.from, function (from) {
          if (from === fromCollection) {
            _.each(edgeDefinition.to, function (to) {
              if (to === toCollection) {
                array.push(edgeDefinition.collection);
              }
            });
          }
        });
      });

      return array;
    },

    initializeGraph: function (sigmaInstance, graph) {
      // var self = this;
      // sigmaInstance.graph.read(graph);
      sigmaInstance.refresh();

      /*
         this.Sigma.plugins.Lasso = sigma.plugins.lasso;

         var lasso = new this.Sigma.plugins.Lasso(sigmaInstance, sigmaInstance.renderers[0], {
         'strokeStyle': 'black',
         'lineWidth': 1,
         'fillWhileDrawing': true,
         'fillStyle': 'rgba(41, 41, 41, 0.2)',
         'cursor': 'crosshair'
         });

      // Listen for selectedNodes event
      lasso.bind('selectedNodes', function (event) {
      // Do something with the selected nodes
      var nodes = event.data;

      if (nodes.length === 0) {
      self.selectedNodes = [];
      } else {
      _.each(nodes, function (val, key) {
      self.selectedNodes[key] = val.id;
      });
      }

      var style = 'position: absolute; right: 25px; bottom: 45px;';

      if (!$('#deleteNodes').is(':visible')) {
      $(self.el).append(
      '<button style=" ' + style + ' "id="deleteNodes" class="button-danger fadeIn animated">Delete selected nodes</button>'
      );
      var c = document.getElementById('deleteNodes');
      c.addEventListener('click', self.deleteNodesModal.bind(self), false);
      }

      self.activeNodes = nodes;
      sigmaInstance.refresh();
      });

      return lasso;
      */
    },

    renderGraph: function (graph, toFocus, aqlMode, layout, renderer, edgeType) {
      var self = this;
      this.graphSettings = graph.settings;

      var color = '#2ecc71';

      if (self.ncolor) {
        color = self.ncolor;
      }

      if (graph.edges) {
        if (graph.nodes) {
          if (graph.nodes.length === 0 && graph.edges.length === 0) {
            graph.nodes.push({
              id: graph.settings.startVertex._id,
              label: graph.settings.startVertex._key,
              size: 10,
              color: color,
              x: Math.random(),
              y: Math.random()
            });
          }

          var style = 'position: absolute; left: 25px; bottom: 50px;';
          if (!this.aqlMode) {
            $('#graph-container').append(
              '<div id="objectCount" style="' + style + ' animated fadeIn">' +
                '<span style="margin-right: 10px" class="arangoState"><span id="nodesCount">' + graph.nodes.length + '</span> nodes</span>' +
                '<span class="arangoState"><span id="edgesCount">' + graph.edges.length + '</span> edges</span>' +
              '</div>'
            );
          }
        }
      }
      this.Sigma = sigma;

      // defaults
      if (!layout) {
        self.algorithm = 'force';
      } else {
        self.algorithm = layout;
      }
      if (!renderer) {
        self.renderer = 'canvas';
      } else {
        self.renderer = renderer;
      }

      if (this.graphConfig) {
        if (this.graphConfig.layout) {
          if (!layout) {
            self.algorithm = this.graphConfig.layout;
          }
        }

        if (this.graphConfig.renderer) {
          if (!renderer) {
            self.renderer = this.graphConfig.renderer;
          }
        }
      }

      if (self.renderer === 'canvas') {
        self.isEditable = true;
      }

      // sigmajs graph settings
      var settings = {
        scalingMode: 'inside',
        borderSize: 3,
        defaultNodeBorderColor: '#8c8c8c',
        doubleClickEnabled: false,
        minNodeSize: 5,
        labelThreshold: 9,
        maxNodeSize: 15,
        batchEdgesDrawing: true,
        minEdgeSize: 1,
        maxEdgeSize: 1,
        enableEdgeHovering: true,
        edgeHoverColor: '#8c8c8c',
        defaultEdgeHoverColor: '#8c8c8c',
        defaultEdgeType: 'arrow',
        edgeHoverSizeRatio: 2.5,
        edgeHoverExtremities: true,
        nodesPowRatio: 0.5,
        // edgesPowRatio: 1.5,
        // lasso settings
        autoRescale: true,
        mouseEnabled: true,
        touchEnabled: true,
        approximateLabelWidth: true,
        font: 'Roboto'
      };

      // halo settings
      // settings.nodeHaloColor = '#FF7A7A';
      settings.nodeHaloColor = 'rgba(146,197,192, 0.8)';
      settings.nodeHaloStroke = false;
      settings.nodeHaloStrokeColor = '#000';
      settings.nodeHaloStrokeWidth = 0;
      settings.nodeHaloSize = 25;
      settings.nodeHaloClustering = false;
      settings.nodeHaloClusteringMaxRadius = 1000;
      settings.edgeHaloColor = '#fff';
      settings.edgeHaloSize = 10;
      settings.drawHalo = true;

      if (self.renderer === 'canvas') {
        settings.autoCurveSortByDirection = true;
      }

      // adjust display settings for big graphs
      if (graph.nodes) {
        if (graph.nodes.length > 250) {
          settings.hideEdgesOnMove = true;
        }
      }

      if (this.graphConfig) {
        if (this.graphConfig.edgeType) {
          settings.defaultEdgeType = this.graphConfig.edgeType;
        }
      }

      if (edgeType) {
        settings.defaultEdgeType = edgeType;
      }
      if (settings.defaultEdgeType === 'arrow') {
        settings.minArrowSize = 7;
      }

      if (aqlMode) {
        // aql editor settings
        self.renderer = 'canvas';

        if (graph.nodes.length < 500) {
          self.algorithm = 'fruchtermann';
        } else {
          settings.scalingMode = 'outside';
        }

        settings.drawEdgeLabels = false;
        settings.minNodeSize = 2;
        settings.maxNodeSize = 8;
      }

      // adjust display settings for webgl renderer
      if (self.renderer === 'webgl') {
        settings.enableEdgeHovering = false;
      }

      // create sigma graph
      var s = new this.Sigma({
        graph: graph,
        container: 'graph-container',
        renderer: {
          container: document.getElementById('graph-container'),
          type: self.renderer
        },
        settings: settings
      });
      this.currentGraph = s;

      if (!this.aqlMode) {
        sigma.plugins.fullScreen({
          container: 'graph-container',
          btnId: 'graph-fullscreen-btn'
        });
      }

      s.graph.nodes().forEach(function (n) {
        n.originalColor = n.color;
      });
      s.graph.edges().forEach(function (e) {
        e.originalColor = e.color;
      });

      if (self.algorithm === 'noverlap') {
        var noverlapListener = s.configNoverlap({
          nodeMargin: 0.1,
          scaleNodes: 1.05,
          gridSize: 75,
          easing: 'quadraticInOut', // animation transition function
          duration: 1500 // animation duration
        });

        noverlapListener.bind('start stop interpolate', function (e) {
          if (e.type === 'start') {
          }
          if (e.type === 'interpolate') {
          }
        });
      } else if (self.algorithm === 'fruchtermann') {
        var frListener = sigma.layouts.fruchtermanReingold.configure(s, {
          iterations: 100,
          easing: 'quadraticInOut',
          duration: 1500
        });

        frListener.bind('start stop interpolate', function (e) {});
      }

      // for canvas renderer allow graph editing
      if (!self.aqlMode) {
        var showAttributes = function (e, node) {
          $('.nodeInfoDiv').remove();

          if (self.contextState.createEdge === false) {
            if (window.location.hash.indexOf('graph') > -1) {
              var callback = function (error, data, id) {
                if (!error) {
                  var attributes = '';
                  attributes += '<span class="title">ID </span> <span class="nodeId">' + data.documents[0]._id + '</span>';
                  if (Object.keys(data.documents[0]).length > 3) {
                    attributes += '<span class="title">ATTRIBUTES </span>';
                  }
                  _.each(data.documents[0], function (value, key) {
                    if (key !== '_key' && key !== '_id' && key !== '_rev' && key !== '_from' && key !== '_to') {
                      attributes += '<span class="nodeAttribute">' + key + '</span>';
                    }
                  });
                  var string = '<div id="nodeInfoDiv" class="nodeInfoDiv" style="display: none;">' + attributes + '</div>';

                  $('#graph-container').append(string);
                  if (self.isFullscreen) {
                    $('.nodeInfoDiv').css('top', '10px');
                    $('.nodeInfoDiv').css('left', '10px');
                  }
                  $('#nodeInfoDiv').fadeIn('slow');
                } else {
                  // node not available any more
                  self.currentGraph.graph.dropNode(id);
                  // rerender graph
                  self.currentGraph.refresh();
                }
              };

              if (node) {
                self.documentStore.getDocument(e.data.node.id.split('/')[0], e.data.node.id.split('/')[1], callback);
              } else {
                self.documentStore.getDocument(e.data.edge.id.split('/')[0], e.data.edge.id.split('/')[1], callback);
              }
            }
          }
        };

        s.bind('clickNode', function (e) {
          if (self.contextState.createEdge === true) {
            self.clearMouseCanvas();
            self.removeHelp();

            // create the edge
            self.contextState._to = e.data.node.id;
            var fromCollection = self.contextState._from.split('/')[0];
            var toCollection = self.contextState._to.split('/')[0];

            // validate edgeDefinitions
            var foundEdgeDefinitions = self.getEdgeDefinitionCollections(fromCollection, toCollection);
            if (foundEdgeDefinitions.length === 0) {
              arangoHelper.arangoNotification('Graph', 'No valid edge definition found.');
            } else {
              self.addEdgeModal(foundEdgeDefinitions, self.contextState._from, self.contextState._to);
              self.clearOldContextMenu(false);
            }
          } else {
            if (!self.dragging) {
              if (self.contextState.createEdge === true) {
                self.newEdgeColor = '#ff0000';
              } else {
                self.newEdgeColor = '#000000';
              }

              // halo on active nodes:
              if (self.renderer === 'canvas') {
                self.currentGraph.renderers[0].halo({
                  nodes: self.currentGraph.graph.nodes(),
                  nodeHaloColor: '#DF0101',
                  nodeHaloSize: 100
                });
              }

              showAttributes(e, true);
              self.activeNodes = [e.data.node];

              if (self.renderer === 'canvas') {
                s.renderers[0].halo({
                  nodes: [e.data.node]
                });
              }

              self.createNodeContextMenu(e.data.node.id, e);
            }
          }
        });

        if (!self.noDefinedGraph) {
          s.bind('clickStage', function (e) {
            if (e.data.captor.isDragging) {
              self.clearOldContextMenu(true);
              self.clearMouseCanvas();
            } else if (self.contextState.createEdge === true) {
              self.clearOldContextMenu(true);
              self.clearMouseCanvas();
              self.removeHelp();
            } else {
              // stage menu
              if (!$('#nodeContextMenu').is(':visible')) {
                // var offset = $('#graph-container').offset();
                self.addNodeX = e.data.captor.x;
                self.addNodeY = e.data.captor.y;
                // self.calculateAddNodePosition(self.cursorX, self.cursorY);
                // self.addNodeX = sigma.utils.getX(e) - offset.left / 2;
                // self.addNodeY = sigma.utils.getY(e) - offset.top / 2;
                // self.addNodeX = e.data.captor.x;
                // self.addNodeY = e.data.captor.y;
                self.createContextMenu(e);
                self.clearMouseCanvas();
              } else {
                // cleanup
                self.clearOldContextMenu(true);
                self.clearMouseCanvas();
                self.removeOldContextMenu();
              }

              // remember halo
              s.renderers[0].halo({
                nodes: self.activeNodes
              });
            }
          });
        } else {
          s.bind('clickStage', function (e) {
            self.clearOldContextMenu(true);
            self.clearMouseCanvas();
            self.removeHelp();
          });
        }
      }

      if (self.renderer === 'canvas') {
        // render parallel edges
        if (this.graphConfig) {
          if (this.graphConfig.edgeType === 'curve') {
            sigma.canvas.edges.autoCurve(s);
          }
        }

        s.bind('clickEdge', function (e) {
          showAttributes(e, false);
        });

        s.renderers[0].bind('render', function (e) {
          s.renderers[0].halo({
            nodes: self.activeNodes
          });
        });

        var unhighlightNodes = function () {
          self.nodeHighlighted = false;
          self.activeNodes = [];

          s.graph.nodes().forEach(function (n) {
            n.color = n.originalColor;
          });

          s.graph.edges().forEach(function (e) {
            e.color = e.originalColor;
          });

          $('.nodeInfoDiv').remove();
          s.refresh({ skipIndexation: true });
        };

        s.bind('rightClickStage', function (e) {
          self.nodeHighlighted = 'undefinedid';
          unhighlightNodes();
        });

        s.bind('rightClickNode', function (e) {
          if (self.nodeHighlighted !== e.data.node.id) {
            var nodeId = e.data.node.id;
            var toKeep = s.graph.neighbors(nodeId);
            toKeep[nodeId] = e.data.node;

            s.graph.nodes().forEach(function (n) {
              if (toKeep[n.id]) {
                n.color = n.originalColor;
              } else {
                n.color = '#eee';
              }
            });

            s.graph.edges().forEach(function (e) {
              if (toKeep[e.source] && toKeep[e.target]) {
                e.color = 'rgb(64, 74, 83)';
              } else {
                e.color = '#eee';
              }
            });

            self.nodeHighlighted = true;
            s.refresh({ skipIndexation: true });
          } else {
            unhighlightNodes();
          }
        });

        if (this.graphConfig) {
          if (this.graphConfig.edgeEditable) {
            s.bind('clickEdge', function (e) {
              var edgeId = e.data.edge.id;
              self.createEdgeContextMenu(edgeId, e);
            });
          }
        }
      }

      // Initialize the dragNodes plugin:
      if (self.algorithm === 'noverlap') {
        s.startNoverlap();
        // allow draggin nodes
      } else if (self.algorithm === 'force') {
        // add buttons for start/stopping calculation
        var style2 = 'color: rgb(64, 74, 83); cursor: pointer; position: absolute; right: 30px; bottom: 40px; z-index: 9999;';

        if (self.aqlMode) {
          style2 = 'color: rgb(64, 74, 83); cursor: pointer; position: absolute; right: 30px; margin-top: 10px; margin-right: -15px';
        }

        $('#graph-container').after(
          '<div id="toggleForce" style="' + style2 + '">' +
            '<i style="margin-right: 5px;" class="fa fa-pause"></i><span> Stop layout</span>' +
          '</div>'
        );
        self.startLayout();

        // suggestion rendering time
        var duration = 250;
        var adjust = 500;
        if (graph.nodes) {
          duration = graph.nodes.length;
          if (aqlMode) {
            if (duration < 250) {
              duration = 250;
            } else {
              duration += adjust;
            }
          } else {
            if (duration <= 250) {
              duration = 500;
            }
            duration += adjust;
          }
        }

        if (graph.empty) {
          arangoHelper.arangoNotification('Graph', 'Your graph is empty. Click inside the white window to create your first node.');
        }

        window.setTimeout(function () {
          self.stopLayout();
        }, duration);
      } else if (self.algorithm === 'fruchtermann') {
        // Start the Fruchterman-Reingold algorithm:
        sigma.layouts.fruchtermanReingold.start(s);
      }

      if (self.algorithm !== 'force') {
        self.reInitDragListener();
      }

      // add listener to keep track of cursor position
      var c = document.getElementsByClassName('sigma-mouse')[0];
      c.addEventListener('mousemove', self.trackCursorPosition.bind(this), false);

      // focus last input box if available
      if (toFocus) {
        $('#' + toFocus).focus();
        $('#graphSettingsContent').animate({
          scrollTop: $('#' + toFocus).offset().top
        }, 2000);
      }

      // clear up info div
      $('#calculatingGraph').fadeOut('slow');

      self.calcFinished = new Date();
      // console.log('Client side calculation took ' + Math.abs(self.calcFinished.getTime() - self.calcStart.getTime()) + ' ms');
      if (graph.empty === true) {
        $('.sigma-background').before('<span id="emptyGraph" style="position: absolute; margin-left: 10px; margin-top: 10px;">The graph is empty. Please right-click to add a node.<span>');
      }
      if (self.graphNotInitialized === true) {
        self.updateColors(self.tmpGraphArray);
        self.graphNotInitialized = false;
        self.tmpGraphArray = [];
      }

      if (self.algorithm === 'force') {
        $('#toggleForce').fadeIn('fast');
      } else {
        $('#toggleForce').fadeOut('fast');
      }
    },

    reInitDragListener: function () {
      var self = this;

      if (this.dragListener !== undefined) {
        sigma.plugins.killDragNodes(this.currentGraph);
        this.dragListener = {};
      }

      // drag nodes listener
      this.dragListener = sigma.plugins.dragNodes(this.currentGraph, this.currentGraph.renderers[0]);

      this.dragListener.bind('drag', function (event) {
        self.dragging = true;
      });

      this.dragListener.bind('drop', function (event) {
        window.setTimeout(function () {
          self.dragging = false;
        }, 400);
      });
    },

    keyUpFunction: function (event) {
      var self = this;
      switch (event.keyCode) {
        case 76:
          if (event.altKey) {
            self.toggleLasso();
          }
          break;
      }
    },

    toggleLayout: function () {
      if (this.layouting) {
        this.stopLayout();
      } else {
        this.startLayout();
      }
    },

    startLayout: function (kill, origin) {
      var self = this;
      this.currentGraph.settings('drawLabels', false);
      this.currentGraph.settings('drawEdgeLabels', false);
      sigma.plugins.killDragNodes(this.currentGraph);

      if (kill === true) {
        this.currentGraph.killForceAtlas2();

        window.setTimeout(function () {
          self.stopLayout();

          if (origin) {
            self.currentGraph.refresh({ skipIndexation: true });
            // self.cameraToNode(origin, 1000);
          }
        }, 500);
      }

      $('#toggleForce .fa').removeClass('fa-play').addClass('fa-pause');
      $('#toggleForce span').html('Stop layout');
      this.layouting = true;
      if (this.aqlMode) {
        this.currentGraph.startForceAtlas2({
          worker: true
        });
      } else {
        this.currentGraph.startForceAtlas2({
          worker: true
        });
      }
      // sigma.plugins.dragNodes(this.currentGraph, this.currentGraph.renderers[0]);
    },

    stopLayout: function () {
      $('#toggleForce .fa').removeClass('fa-pause').addClass('fa-play');
      $('#toggleForce span').html('Resume layout');
      this.layouting = false;
      this.currentGraph.stopForceAtlas2();
      this.currentGraph.settings('drawLabels', true);
      this.currentGraph.settings('drawEdgeLabels', true);
      this.currentGraph.refresh({ skipIndexation: true });
      this.reInitDragListener();
    }

  });
}());
