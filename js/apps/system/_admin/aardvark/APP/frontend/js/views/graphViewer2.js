/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, _, frontendConfig, slicePath, icon, Joi, wheelnav, document, sigma, Backbone, templateEngine, $, window*/
(function () {
  'use strict';

  window.GraphViewer2 = Backbone.View.extend({
    el: '#content',

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      return this;
    },

    template: templateEngine.createTemplate('graphViewer2.ejs'),

    initialize: function (options) {
      var self = this;

      if (options.id) {
        // dynamically set id if available
        this.setElement(options.id);
        this.graphData = options.data;
        this.aqlMode = true;
      }

      this.name = options.name;
      this.userConfig = options.userConfig;
      this.documentStore = options.documentStore;
      this.initSigma();

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
      random1: ['#292F36', '#4ECDC4', '#F7FFF7', '#DD6363', '#FFE66D']
    },

    activeNodes: [],
    selectedNodes: {},

    aqlMode: false,

    events: {
      'click #downloadPNG': 'downloadPNG',
      'click #reloadGraph': 'reloadGraph',
      'click #settingsMenu': 'toggleSettings',
      'click #noGraphToggle': 'toggleSettings',
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

    initSigma: function () {
      // init sigma
      try {
        sigma.classes.graph.addMethod('neighbors', function (nodeId) {
          var k;
          var neighbors = {};
          var index = this.allNeighborsIndex[nodeId] || {};

          for (k in index) {
            neighbors[k] = this.nodesIndex[k];
          }
          return neighbors;
        });

        sigma.classes.graph.addMethod('getNodeEdges', function (nodeId) {
          var edges = this.edges();
          var edgesToReturn = [];

          _.each(edges, function (edge) {
            if (edge.source === nodeId || edge.target === nodeId) {
              edgesToReturn.push(edge.id);
            }
          });
          return edgesToReturn;
        });

        sigma.classes.graph.addMethod('getNodeEdgesCount', function (id) {
          return this.allNeighborsCount[id];
        });

        sigma.classes.graph.addMethod('getNodesCount', function () {
          return this.nodesArray.length;
        });
      } catch (ignore) {}
    },

    downloadPNG: function () {
      var size = parseInt($('#graph-container').width(), 10);
      sigma.plugins.image(this.currentGraph, this.currentGraph.renderers[0], {
        download: true,
        size: size,
        labels: true,
        background: 'white',
        zoom: true
      });
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

              $('#objectCount').css('bottom', '10px');
              $('#objectCount').css('left', '25px');
            } else {
              self.isFullscreen = false;

              // NO FULLSCREEN STYLING
              $('#toggleForce').css('bottom', '40px');

              $('#objectCount').css('bottom', '50px');
              $('#objectCount').css('left', '25px');
            }
            console.log(self.isFullscreen);
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

    renderAQL: function (data) {
      this.$el.html(this.template.render({}));

      // remove not needed elements
      this.$el.find('.headerBar').remove();

      // set graph box height
      var height = $('.centralRow').height() - 250;
      this.$el.find('#graph-container').css('height', height);

      // render
      this.graphData.modified = this.parseData(this.graphData.original, this.graphData.graphInfo);
      this.renderGraph(this.graphData.modified, null, true);
    },

    parseData: function (data, type) {
      var vertices = {}; var edges = {};
      var returnObj = {
        nodes: [],
        edges: [],
        settings: {}
      };

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
              vertices[node._id] = {
                id: node._id,
                label: node._key,
                // size: 0.3,
                color: '#2ecc71',
                x: Math.random(),
                y: Math.random()
              };
            });
          }
        });

        _.each(vertices, function (node) {
          returnObj.nodes.push(node);
        });

        _.each(edges, function (edge) {
          returnObj.edges.push(edge);
        });
      } else if (type === 'array') {
        _.each(data, function (edge) {
          vertices[edge._from] = null;
          vertices[edge._to] = null;

          returnObj.edges.push({
            id: edge._id,
            source: edge._from,
            // label: edge._key,
            color: '#cccccc',
            target: edge._to
          });
        });

        _.each(vertices, function (val, key) {
          returnObj.nodes.push({
            id: key,
            label: key,
            size: 0.3,
            color: '#2ecc71',
            x: Math.random(),
            y: Math.random()
          });
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
        if (this.graphConfig) {
          ajaxData = _.clone(this.graphConfig);

          // remove not needed params
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

        this.setupSigma();

        self.fetchStarted = new Date();
        $.ajax({
          type: 'GET',
          url: arangoHelper.databaseUrl('/_admin/aardvark/graph/' + encodeURIComponent(this.name)),
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
              arangoHelper.arangoError('Graph', e.responseJSON.exception);

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
            } catch (ignore) {}
          }
        });
      }.bind(this);

      // load graph configuration
      this.getGraphSettings(continueFetchGraph);
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

      if ($('#delete-node-edges-attr').val() === 'yes') {
        $.ajax({
          cache: false,
          type: 'DELETE',
          contentType: 'application/json',
          url: arangoHelper.databaseUrl(
            '/_api/gharial/' + encodeURIComponent(self.name) + '/vertex/' + encodeURIComponent(documentKey.split('/')[0]) + '/' + encodeURIComponent(documentKey.split('/')[1])
          ),
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
      var x = self.addNodeX / 100;
      var y = self.addNodeY / 100;

      var collectionId = $('.modal-body #new-node-collection-attr').val();
      var key = $('.modal-body #new-node-key-attr').last().val();

      var callback = function (error, id, msg) {
        if (error) {
          arangoHelper.arangoError('Could not create node', msg.errorMessage);
        } else {
          $('#emptyGraph').remove();
          self.currentGraph.graph.addNode({
            id: id,
            label: id.split('/')[1] || '',
            size: self.graphConfig.nodeSize || 15,
            color: self.graphConfig.nodeColor || '#2ecc71',
            x: x,
            y: y
          });

          window.modalView.hide();
          // rerender graph
          self.currentGraph.refresh();
        }
      };

      if (key !== '' || key !== undefined) {
        this.documentStore.createTypeDocument(collectionId, key, callback);
      } else {
        this.documentStore.createTypeDocument(collectionId, null, callback);
      }
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
            value: val.id
          });
        });

        tableContent.push(
          window.modalView.createTextEntry(
            'new-node-key-attr',
            '_key',
            undefined,
            'The nodes unique key(optional attribute, leave empty for autogenerated key',
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

        tableContent.push(
          window.modalView.createSelectEntry(
            'new-node-collection-attr',
            'Collection',
            undefined,
            'Please select the destination for the new node.',
            collections
          )
        );

        buttons.push(
          window.modalView.createSuccessButton('Create', this.addNode.bind(this))
        );

        window.modalView.show(
          'modalTable.ejs',
          'Create node',
          buttons,
          tableContent
        );
      } else {
        arangoHelper.arangoError('Graph', 'No valid vertex collections found.');
      }
    },

    addEdge: function () {
      var self = this;
      var from = self.contextState._from;
      var to = self.contextState._to;

      var collectionName;
      if ($('.modal-body #new-edge-collection-attr').val() === '') {
        collectionName = $('.modal-body #new-edge-collection-attr').text();
      } else {
        collectionName = $('.modal-body #new-edge-collection-attr').val();
      }
      var key = $('.modal-body #new-edge-key-attr').last().val();

      var callback = function (error, data, msg) {
        if (!error) {
          // success
          if (self.graphConfig.edgeEditable === 'true') {
            self.currentGraph.graph.addEdge({
              source: from,
              size: 1,
              target: to,
              id: data._id,
              color: self.graphConfig.edgeColor
            });
          } else {
            self.currentGraph.graph.addEdge({
              source: from,
              target: to,
              id: data._id,
              color: self.graphConfig.edgeColor
            });
          }

          // rerender graph
          if (self.graphConfig) {
            if (self.graphConfig.edgeType === 'curve') {
              sigma.canvas.edges.autoCurve(self.currentGraph);
            }
          }
          self.currentGraph.refresh();
        } else {
          arangoHelper.arangoError('Could not create edge', msg.errorMessage);
        }

        // then clear states
        self.clearOldContextMenu(true);
        window.modalView.hide();
      };

      if (key !== '' || key !== undefined) {
        this.documentStore.createTypeEdge(collectionName, from, to, key, callback);
      } else {
        this.documentStore.createTypeEdge(collectionName, from, to, null, callback);
      }
    },

    addEdgeModal: function (edgeDefinitions) {
      if (edgeDefinitions !== 0) {
        var buttons = []; var tableContent = [];

        tableContent.push(
          window.modalView.createTextEntry(
            'new-edge-key-attr',
            '_key',
            undefined,
            'The edges unique key(optional attribute, leave empty for autogenerated key',
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
              'Please select the destination for the new edge.',
              collections
            )
          );
        } else {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'new-edge-collection-attr',
              'Edge collection',
              edgeDefinitions[0],
              'The edges collection to be used.'
            )
          );
        }

        buttons.push(
          window.modalView.createSuccessButton('Create', this.addEdge.bind(this))
        );

        window.modalView.show(
          'modalTable.ejs',
          'Create edge',
          buttons,
          tableContent
        );
      } else {
        arangoHelper.arangoError('Graph', 'No valid edge definitions found.');
      }
    },

    updateColors: function (nodes, edges, ncolor, ecolor) {
      var combinedName = window.App.currentDB.toJSON().name + '_' + this.name;
      var self = this;

      this.userConfig.fetch({
        success: function (data) {
          if (nodes === true) {
            self.graphConfig = data.toJSON().graphs[combinedName];
            self.currentGraph.graph.nodes().forEach(function (n) {
              n.color = ncolor;
            });
          }

          if (edges === true) {
            self.currentGraph.graph.edges().forEach(function (e) {
              e.color = ecolor;
            });
          }

          self.currentGraph.refresh();
        }
      });
    },

    nodesContextMenuCheck: function (e) {
      this.nodesContextEventState = e;
      this.openNodesDate = new Date();
    },

    // click nodes context menu
    /*
    createNodesContextMenu: function () {
      var self = this;
      var e = self.nodesContextEventState;

      var x = e.clientX - 50;
      var y = e.clientY - 50;
      self.clearOldContextMenu();

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
        if (self.viewStates.captureMode) {
          wheel.createWheel([icon.plus, icon.trash]);
        } else {
          wheel.createWheel([icon.trash, icon.arrowleft2]);
        }

        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;
        // add menu events

        // function 0: remove all selectedNodes
        wheel.navItems[0].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.deleteNodesModal();
        };

        // function 1: clear contextmenu
        wheel.navItems[1].navigateFunction = function (e) {
          self.clearOldContextMenu();
        };

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
    */

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
        };

        // function 1: exit
        wheel.navItems[1].navigateFunction = function (e) {
          self.clearOldContextMenu();
        };

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
        wheel.createWheel([icon.edit, icon.trash]);

        // add menu events
        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;

        // function 0: edit
        wheel.navItems[0].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.editEdge(edgeId);
        };

        // function 1: delete
        wheel.navItems[1].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.deleteEdgeModal(edgeId);
        };

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
        wheel.createWheel([
          'imgsrc:img/gv_edit.png',
          'imgsrc:img/gv_trash.png',
          'imgsrc:img/gv_flag.png',
          'imgsrc:img/gv_link.png',
          'imgsrc:img/gv_expand.png'
        ]);

        $('#nodeContextMenu').addClass('animated bounceIn');

        window.setTimeout(function () {
          // add menu events

          // function 0: edit
          wheel.navItems[0].navigateFunction = function (e) {
            self.clearOldContextMenu();
            self.editNode(nodeId);
          };

          // function 1: delete
          wheel.navItems[1].navigateFunction = function (e) {
            self.clearOldContextMenu();
            self.deleteNodeModal(nodeId);
          };

          // function 2: mark as start node
          wheel.navItems[2].navigateFunction = function (e) {
            self.clearOldContextMenu();
            self.setStartNode(nodeId);
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
          };

          // function 4: mark as start node
          wheel.navItems[4].navigateFunction = function (e) {
            self.clearOldContextMenu();
            self.expandNode(nodeId);
          };

          // add menu hover functions

          var descriptions = [
            'Edit the node.',
            'Delete node.',
            'Set as startnode.',
            'Draw edge.',
            'Expand the node.'
          ];

          // hover functions
          _.each(descriptions, function (val, key) {
            wheel.navItems[key].navTitle.mouseover(function () { self.drawHelp(val); });
            wheel.navItems[key].navTitle.mouseout(function () { self.removeHelp(); });
          });

          /* TODO
          wheel.navItems[0].navSlice.mouseover(function (a) {
            $(a.target).css('opacity', '1');
          });
          wheel.navItems[0].navSlice.mouseout(function (a) {
            $(a.target).css('opacity', '0.8');
          });
          */

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

      ajaxData.query = 'FOR v, e, p IN 1..1 ANY "' + id + '" GRAPH "' + self.name + '" RETURN p';

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
                existingNode.label = existingNode.label + ' (expanded)';
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

          _.each(newEdges, function (edge) {
            if (edge.source === newNode.id || edge.target === newNode.id) {
              edge.originalColor = edge.color;
              self.currentGraph.graph.addEdge(edge);
              newEdgeCounter++;
            }
          });
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
          // self.cameraToNode(origin);
        } else if (self.algorithm === 'noverlap') {
          self.startLayout(true, origin); // tmp bugfix, rerender with noverlap currently not possible
          // self.currentGraph.startNoverlap();
        }
      }
    },

    /*
    cameraToNode: function (node) {
      var self = this;
      console.log(node);

      self.currentGraph.cameras[0].goTo({
        x: node['read_cam0:x'],
        y: node['read_cam0:y']
      });
      /*
      sigma.misc.animation.camera(
        self.currentGraph.camera,
        {
          x: node[self.currentGraph.camera.readPrefix + 'x'],
          y: node[self.currentGraph.camera.readPrefix + 'y'],
          ratio: 1
        },
        {duration: self.currentGraph.settings('animationsTime') || 300}
      );
    },
    */

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
      var combinedName = frontendConfig.db + '_' + this.name;

      this.userConfig.fetch({
        success: function (data) {
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
          self.graphSettingsView.render();

          if (callback) {
            callback(self.graphConfig);
          }
        }
      });
    },

    setStartNode: function (id) {
      this.graphConfig.nodeStart = id;
      this.graphSettingsView.saveGraphSettings(undefined, undefined, id);
    },

    editNode: function (id) {
      var callback = function () {};
      arangoHelper.openDocEditor(id, 'doc', callback);
    },

    editEdge: function (id) {
      var callback = function () {};
      arangoHelper.openDocEditor(id, 'edge', callback);
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

    renderGraph: function (graph, toFocus, aqlMode) {
      var self = this;

      this.graphSettings = graph.settings;

      if (graph.edges) {
        if (graph.nodes) {
          if (graph.nodes.length === 0 && graph.edges.length === 0) {
            graph.nodes.push({
              id: graph.settings.startVertex._id,
              label: graph.settings.startVertex._key,
              size: 10,
              color: '#2ecc71',
              x: Math.random(),
              y: Math.random()
            });
          }

          var style = 'position: absolute; left: 25px; bottom: 50px;';
          if (this.aqlMode) {
            style = 'position: absolute; left: 30px; margin-top: -37px;';
          }

          $('#graph-container').append(
            '<div id="objectCount" style="' + style + ' animated fadeIn">' +
              '<span style="margin-right: 10px" class="arangoState"><span id="nodesCount">' + graph.nodes.length + '</span> nodes</span>' +
                '<span class="arangoState"><span id="edgesCount">' + graph.edges.length + '</span> edges</span>' +
                  '</div>'
          );
        }
      }
      this.Sigma = sigma;

      // defaults
      var algorithm = 'force';
      var renderer = 'canvas';

      if (this.graphConfig) {
        if (this.graphConfig.layout) {
          algorithm = this.graphConfig.layout;
        }

        if (this.graphConfig.renderer) {
          renderer = this.graphConfig.renderer;

          if (renderer === 'canvas') {
            self.isEditable = true;
          }
        }
      }

      // sigmajs graph settings
      var settings = {
        scalingMode: 'inside',
        borderSize: 3,
        defaultNodeBorderColor: '#8c8c8c',
        doubleClickEnabled: false,
        minNodeSize: 5,
        labelThreshold: 10,
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

      if (renderer === 'canvas') {
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

          if (this.graphConfig.edgeType === 'arrow') {
            settings.minArrowSize = 7;
          }
        }
      }

      if (aqlMode) {
        // aql editor settings
        renderer = 'webgl';

        if (graph.nodes.length < 500) {
          algorithm = 'fruchtermann';
        } else {
          settings.scalingMode = 'outside';
        }

        settings.drawEdgeLabels = false;
        settings.minNodeSize = 2;
        settings.maxNodeSize = 8;
      }

      // adjust display settings for webgl renderer
      if (renderer === 'webgl') {
        settings.enableEdgeHovering = false;
      }

      // create sigma graph
      var s = new this.Sigma({
        graph: graph,
        container: 'graph-container',
        renderer: {
          container: document.getElementById('graph-container'),
          type: renderer
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

      if (algorithm === 'noverlap') {
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
      } else if (algorithm === 'fruchtermann') {
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
              var callback = function (error, data) {
                if (!error) {
                  var attributes = '';
                  attributes += '<span>ID </span> <span class="nodeId">' + data._id + '</span>';
                  if (Object.keys(data).length > 3) {
                    attributes += '<span>ATTRIBUTES </span>';
                  }
                  _.each(data, function (value, key) {
                    if (key !== '_key' && key !== '_id' && key !== '_rev' && key !== '_from' && key !== '_to') {
                      attributes += '<span class="nodeAttribute">' + key + '</span>';
                    }
                  });
                  var string = '<div id="nodeInfoDiv" class="nodeInfoDiv" style="display: none;">' + attributes + '</div>';

                  $('#graph-container').append(string);
                  $('#nodeInfoDiv').fadeIn('slow');
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
            self.addEdgeModal(foundEdgeDefinitions, self.contextState._from, self.contextState._to);
            self.clearOldContextMenu(true);
          } else {
            if (!self.dragging) {
              if (self.contextState.createEdge === true) {
                self.newEdgeColor = '#ff0000';
              } else {
                self.newEdgeColor = '#000000';
              }

              // halo on active nodes:
              if (renderer === 'canvas') {
                self.currentGraph.renderers[0].halo({
                  nodes: self.currentGraph.graph.nodes(),
                  nodeHaloColor: '#DF0101',
                  nodeHaloSize: 100
                });
              }

              showAttributes(e, true);
              self.activeNodes = [e.data.node];

              if (renderer === 'canvas') {
                s.renderers[0].halo({
                  nodes: [e.data.node]
                });
              }

              if (!this.aqlMode) {
                self.createNodeContextMenu(e.data.node.id, e);
              }
            }
          }
        });

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
              var offset = $('#graph-container').offset();
              self.addNodeX = sigma.utils.getX(e) - offset.left / 2;
              self.addNodeY = sigma.utils.getY(e) - offset.top / 2;
              // self.addNodeX = e.data.captor.x;
              // self.addNodeY = e.data.captor.y;
              self.createContextMenu(e);
              self.clearMouseCanvas();
            } else {
              // cleanup
              self.clearOldContextMenu(true);
              self.clearMouseCanvas();
            }

            // remember halo
            s.renderers[0].halo({
              nodes: self.activeNodes
            });
          }
        });
      }

      if (renderer === 'canvas') {
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
          if (s.graph.getNodesCount() < 250) {
            self.activeNodes = [];

            s.graph.nodes().forEach(function (n) {
              n.color = n.originalColor;
            });

            s.graph.edges().forEach(function (e) {
              e.color = e.originalColor;
            });

            $('.nodeInfoDiv').remove();
            s.refresh({ skipIndexation: true });
          }
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

      // store in view
      self.algorithm = algorithm;

      // Initialize the dragNodes plugin:
      if (algorithm === 'noverlap') {
        s.startNoverlap();
        // allow draggin nodes
      } else if (algorithm === 'force') {
        // add buttons for start/stopping calculation
        var style2 = 'color: rgb(64, 74, 83); cursor: pointer; position: absolute; right: 30px; bottom: 40px;';

        if (self.aqlMode) {
          style2 = 'color: rgb(64, 74, 83); cursor: pointer; position: absolute; right: 30px; margin-top: -30px;';
        }

        $('#graph-container').append(
          '<div id="toggleForce" style="' + style2 + '">' +
            '<i style="margin-right: 5px;" class="fa fa-pause"></i><span> Stop layout</span>' +
          '</div>'
        );
        self.startLayout();

        // suggestion rendering time
        var duration = graph.nodes.length;

        if (aqlMode) {
          if (duration < 250) {
            duration = 250;
          }
        } else {
          if (duration <= 250) {
            duration = 500;
          }
        }

        window.setTimeout(function () {
          self.stopLayout();
        }, duration);
      } else if (algorithm === 'fruchtermann') {
        // Start the Fruchterman-Reingold algorithm:
        sigma.layouts.fruchtermanReingold.start(s);
      }
      var dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);

      dragListener.bind('drag', function (event) {
        self.dragging = true;
      });
      dragListener.bind('drop', function (event) {
        window.setTimeout(function () {
          self.dragging = false;
        }, 400);
      });

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

      /*
      var enableLasso = function () {
        self.graphLasso = self.initializeGraph(s, graph);
        self.graphLasso.activate();
        self.graphLasso.deactivate();
      };

      // init graph lasso
      if (this.graphConfig) {
        if (this.graphConfig.renderer === 'canvas') {
          enableLasso();
        } else {
          $('#selectNodes').parent().hide();
        }
      } else {
        if (renderer === 'canvas') {
          enableLasso();
        } else {
          $('#selectNodes').parent().hide();
        }
      }
      */

      /* if (self.graphLasso) {
        // add lasso event
        // Toggle lasso activation on Alt + l
        window.App.listenerFunctions['graphViewer'] = this.keyUpFunction.bind(this);
      } */

      // clear up info div
      $('#calculatingGraph').fadeOut('slow');

      if (!aqlMode) {
        if (self.graphConfig) {
          if (self.graphConfig.nodeSizeByEdges === 'false') {
            // make nodes a bit bigger
            // var maxNodeSize = s.settings('maxNodeSize');
            // var factor = 1;
            // var length = s.graph.nodes().length;

            /*
            factor = 0.35;
            maxNodeSize = maxNodeSize * factor;
            s.settings('maxNodeSize', maxNodeSize);
            s.refresh({});
           */
          }
        }
      }

      self.calcFinished = new Date();
      // console.log('Client side calculation took ' + Math.abs(self.calcFinished.getTime() - self.calcStart.getTime()) + ' ms');
      if (graph.empty === true) {
        $('.sigma-background').before('<span id="emptyGraph" style="position: absolute; margin-left: 10px; margin-top: 10px;">The graph is empty. Please right-click to add a node.<span>');
      }
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

    /*
    toggleLasso: function () {
      var self = this;

      if (this.graphLasso.isActive) {
        var y = document.getElementById('deleteNodes');
        y.removeEventListener('click', self.deleteNodesModal, false);
        $('#deleteNodes').remove();

        // remove event
        var c = document.getElementsByClassName('sigma-lasso')[0];
        c.removeEventListener('mouseup', this.nodesContextMenuCheck.bind(this), false);

        $('#selectNodes').removeClass('activated');
        this.graphLasso.deactivate();

        // clear selected nodes state
        this.selectedNodes = {};
        this.activeNodes = [];
        this.currentGraph.refresh({ skipIndexation: true });
      } else {
        $('#selectNodes').addClass('activated');
        this.graphLasso.activate();

        // add event
        var x = document.getElementsByClassName('sigma-lasso')[0];
        x.addEventListener('mouseup', self.nodesContextMenuCheck.bind(this), false);
      }
    },
    */

    startLayout: function (kill, origin) {
      var self = this;
      this.currentGraph.settings('drawLabels', false);
      this.currentGraph.settings('drawEdgeLabels', false);

      if (kill === true) {
        this.currentGraph.killForceAtlas2();

        window.setTimeout(function () {
          self.stopLayout();

          if (origin) {
            self.currentGraph.refresh();
            // self.cameraToNode(origin);
          }
        }, 500);
      }

      $('#toggleForce .fa').removeClass('fa-play').addClass('fa-pause');
      $('#toggleForce span').html('Stop layout');
      this.layouting = true;
      if (this.aqlMode) {
        this.currentGraph.startForceAtlas2({
          worker: true,
          edgeWeightInfluence: 2
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
      sigma.plugins.dragNodes(this.currentGraph, this.currentGraph.renderers[0]);
      this.currentGraph.settings('drawLabels', true);
      this.currentGraph.settings('drawEdgeLabels', true);
      this.currentGraph.refresh({ skipIndexation: true });
    }

  });
}());
