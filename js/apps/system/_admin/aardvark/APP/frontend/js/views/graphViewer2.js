/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, _, slicePath, icon, wheelnav, document, sigma, Backbone, templateEngine, $, window*/
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
      this.name = options.name;
      this.userConfig = options.userConfig;
      this.initSigma();
    },

    events: {
      'click #downloadPNG': 'downloadSVG'
    },

    cursorX: 0,
    cursorY: 0,

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
      } catch (ignore) {}
    },

    downloadSVG: function () {
      var self = this;

      this.currentGraph.toSVG({
        download: true,
        filename: self.name + '.svg',
        size: 1000
      });
    },

    resize: function () {
      // adjust container widht + height
      $('#graph-container').width($('.centralContent').width());
      $('#graph-container').height($('.centralRow').height() - 150);
    },

    render: function () {
      this.$el.html(this.template.render({}));

      this.resize();
      this.fetchGraph();
    },

    fetchGraph: function () {
      var self = this;
      arangoHelper.buildGraphSubNav(self.name, 'Content');
      $('#content').append(
        '<div id="calculatingGraph" style="position: absolute; left: 25px; top: 130px;">' +
        '<i class="fa fa-circle-o-notch fa-spin" style="margin-right: 10px;"></i>' +
        '<span id="calcText">Fetching graph data. Please wait ... </span></br></br></br>' +
        '<span style="font-weight: 100; opacity: 0.6; font-size: 9pt;">If it`s taking too much time to draw the graph, please go to: </br>' +
        '<a href="' + window.location.href + '/settings">' + window.location.href + '/settings </a></br> and adjust your settings.' +
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

        this.setupSigma();

        $.ajax({
          type: 'GET',
          url: arangoHelper.databaseUrl('/_admin/aardvark/graph/' + encodeURIComponent(this.name)),
          contentType: 'application/json',
          data: ajaxData,
          success: function (data) {
            $('#calcText').html('Calculating layout. Please wait ... ');
            arangoHelper.buildGraphSubNav(self.name, 'Content');
            self.renderGraph(data);
          },
          error: function (e) {
            console.log(e);
            try {
              arangoHelper.arangoError('Graph', e.responseJSON.exception);
            } catch (ignore) {}

            $('#calculatingGraph').html('Failed to fetch graph information.');
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
            defaultEdgeLabelSize: 10,
            edgeLabelSize: 'fixed',
            edgeLabelSizePowRatio: 1,
            edgeLabelThreshold: 1
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
      var string = '<div id="nodeContextMenu" class="nodeContextMenu"></div>';
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

      // clear info div
    },

    trackCursorPosition: function (e) {
      this.cursorX = e.x;
      this.cursorY = e.y;
    },

    createContextMenu: function (e) {
      var self = this;
      var x = self.cursorX - 50;
      var y = self.cursorY - 50;
      console.log(e);
      this.clearOldContextMenu();

      var generateMenu = function (e) {
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
        wheel.createWheel([icon.plus, icon.trash]);

        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;
        // add menu events

        // function 0: edit
        wheel.navItems[0].navigateFunction = function (e) {
          self.clearOldContextMenu();
        };

        // function 1: delete
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

    createNodeContextMenu: function (nodeId, e) {
      var self = this;

      // var x = e.data.node['renderer1:x'];
      // var y = e.data.node['renderer1:y'];
      // better to use x,y from top, but sometimes values are not correct ...
      console.log(e);
      var x = e.data.captor.clientX - 52;
      var y = e.data.captor.clientY - 52;
      console.log(e.data);

      this.clearOldContextMenu();

      var generateMenu = function (e, nodeId) {
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
        wheel.createWheel([icon.edit, icon.trash, icon.arrowleft2, icon.connect]);

        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;
        // add menu events

        // function 0: edit
        wheel.navItems[0].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.editNode(nodeId);
        };

        // function 1: delete
        wheel.navItems[1].navigateFunction = function (e) {
          self.clearOldContextMenu();
          self.deleteNode(nodeId);
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
          c.addEventListener('mousemove', self.drawLine.bind(this), false);

          self.clearOldContextMenu();
        };

        // deselect active default entry
        wheel.navItems[0].selected = false;
        wheel.navItems[0].hovered = false;
      };

      $('#nodeContextMenu').css('left', x);
      $('#nodeContextMenu').css('top', y);
      $('#nodeContextMenu').width(100);
      $('#nodeContextMenu').height(100);

      generateMenu(e, nodeId);
    },

    clearMouseCanvas: function () {
      var c = document.getElementsByClassName('sigma-mouse')[0];
      var ctx = c.getContext('2d');
      ctx.clearRect(0, 0, $(c).width(), $(c).height());
    },

    drawLine: function (e) {
      var context = window.App.graphViewer2.contextState;

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
        ctx.stroke();
      }
    },

    getGraphSettings: function (callback) {
      var self = this;
      var combinedName = window.App.currentDB.toJSON().name + '_' + this.name;

      this.userConfig.fetch({
        success: function (data) {
          self.graphConfig = data.toJSON().graphs[combinedName];

          if (callback) {
            callback(self.graphConfig);
          }
        }
      });
    },

    editNode: function (id) {
      var callback = function () {};

      arangoHelper.openDocEditor(id, 'doc', callback);
    },

    renderGraph: function (graph) {
      var self = this;

      if (graph.edges.left === 0) {
        return;
      }

      this.Sigma = sigma;

      // defaults
      var algorithm = 'noverlap';
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

      var settings = {
        doubleClickEnabled: false,
        minNodeSize: 3.5,
        minEdgeSize: 1,
        maxEdgeSize: 4,
        enableEdgeHovering: true,
        edgeHoverColor: '#000',
        defaultEdgeHoverColor: '#000',
        defaultEdgeType: 'line',
        edgeHoverSizeRatio: 2,
        edgeHoverExtremities: true
      };

      if (this.graphConfig) {
        if (this.graphConfig.edgeType) {
          settings.defaultEdgeType = this.graphConfig.edgeType;
        }
      }

      // adjust display settings for big graphs
      if (graph.nodes.length > 500) {
        // show node label if size is 15
        settings.labelThreshold = 15;
        settings.hideEdgesOnMove = true;
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

      sigma.plugins.fullScreen({
        container: 'graph-container',
        btnId: 'graph-fullscreen-btn'
      });

      if (algorithm === 'noverlap') {
        var noverlapListener = s.configNoverlap({
          nodeMargin: 0.1,
          scaleNodes: 1.05,
          gridSize: 75,
          easing: 'quadraticInOut', // animation transition function
          duration: 10000 // animation duration. Long here for the purposes of this example only
        });

        noverlapListener.bind('start stop interpolate', function (e) {
          if (e.type === 'start') {
          }
          if (e.type === 'interpolate') {
          }
        });
      } else if (algorithm === 'fruchtermann') {
        var frListener = sigma.layouts.fruchtermanReingold.configure(s, {
          iterations: 500,
          easing: 'quadraticInOut',
          duration: 800
        });

        frListener.bind('start stop interpolate', function (e) {});
      }

      s.graph.nodes().forEach(function (n) {
        n.originalColor = n.color;
      });
      s.graph.edges().forEach(function (e) {
        e.originalColor = e.color;
      });

      // for canvas renderer allow graph editing
      if (renderer === 'canvas') {
        s.bind('rightClickStage', function (e) {
          self.createContextMenu(e);
          self.clearMouseCanvas();
        });

        s.bind('clickNode', function (e) {
          if (self.contextState.createEdge === true) {
            // create the edge
            self.contextState._to = e.data.node.id;

            self.currentGraph.graph.addEdge({
              source: self.contextState._from,
              target: self.contextState._to,
              id: Math.random(),
              color: self.graphConfig.edgeColor
            });

            // rerender graph
            self.currentGraph.refresh();

            // then clear states
            self.clearOldContextMenu(true);
          }
        });

        s.bind('rightClickNode', function (e) {
          var nodeId = e.data.node.id;
          self.createNodeContextMenu(nodeId, e);
        });

        s.bind('doubleClickNode', function (e) {
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

          s.refresh();
        });

        s.bind('doubleClickStage', function () {
          s.graph.nodes().forEach(function (n) {
            n.color = n.originalColor;
          });

          s.graph.edges().forEach(function (e) {
            e.color = e.originalColor;
          });

          s.refresh();
        });

        s.bind('clickStage', function () {
          self.clearOldContextMenu(true);
          self.clearMouseCanvas();
        });
      }

      var dragListener;
      // Initialize the dragNodes plugin:
      if (algorithm === 'noverlap') {
        s.startNoverlap();
        // allow draggin nodes
        dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);
      } else if (algorithm === 'force') {
        s.startForceAtlas2({worker: true, barnesHutOptimize: false});

        var duration = 3000;

        if (graph.nodes.length > 2500) {
          duration = 5000;
        } else if (graph.nodes.length < 50) {
          duration = 500;
        }

        window.setTimeout(function () {
          s.stopForceAtlas2();
          dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);
        }, duration);
      } else if (algorithm === 'fruchtermann') {
        // Start the Fruchterman-Reingold algorithm:
        sigma.layouts.fruchtermanReingold.start(s);
        dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);
      } else {
        dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);
      }
      console.log(dragListener);

      // add listener to keep track of cursor position
      var c = document.getElementsByClassName('sigma-mouse')[0];
      c.addEventListener('mousemove', self.trackCursorPosition.bind(this), false);

      // clear up info div
      $('#calculatingGraph').remove();
    }

  });
}());
