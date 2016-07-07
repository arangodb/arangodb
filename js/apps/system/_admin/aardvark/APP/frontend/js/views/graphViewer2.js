/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, slicePath, icon, wheelnav, document, sigma, Backbone, templateEngine, $, window*/
(function () {
  'use strict';

  window.GraphViewer2 = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('graphViewer2.ejs'),

    initialize: function (options) {
      this.name = options.name;
      this.initSigma();
    },

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

    render: function () {
      this.$el.html(this.template.render({}));
      arangoHelper.buildGraphSubNav(this.name, 'Content');

      // adjust container widht + height
      $('#graph-container').width($('.centralContent').width());
      $('#graph-container').height($('.centralRow').height() - 150);

      this.fetchGraph();
    },

    fetchGraph: function () {
      var self = this;

      $.ajax({
        type: 'GET',
        url: arangoHelper.databaseUrl('/_admin/aardvark/graph/' + encodeURIComponent(this.name)),
        contentType: 'application/json',
        success: function (data) {
          self.renderGraph(data);
        }
      });
    },

    clearOldContextMenu: function () {
      $('#nodeContextMenu').remove();
    },

    createNodeContextMenu: function (nodeId, e) {
      var self = this;

      var x = e.data.node['renderer1:x'];
      var y = e.data.node['renderer1:y'];

      this.clearOldContextMenu();
      var string = '<div id="nodeContextMenu" class="nodeContextMenu"></div>';
      $('#graph-container').append(string);

      var generateMenu = function (e, nodeId) {
        var hotaru = ['#364C4A', '#497C7F', '#92C5C0', '#858168', '#CCBCA5'];

        var Wheelnav = wheelnav;

        var wheel = new Wheelnav('nodeContextMenu');
        wheel.maxPercent = 1.0;
        wheel.wheelRadius = 50;
        wheel.clockwise = false;
        wheel.colors = hotaru;
        wheel.clickModeRotate = false;
        wheel.slicePathFunction = slicePath().DonutSlice;
        wheel.createWheel([icon.edit, icon.trash, icon.smallgear, icon.smallgear]);

        // add menu events

        // function 0: edit
        wheel.navItems[0].navigateFunction = function () {
          self.clearOldContextMenu();
          self.editNode(nodeId);
        };

        // function 1:
        wheel.navItems[1].navigateFunction = function () {
          self.clearOldContextMenu();
          self.editNode(nodeId);
        };

        // function 2:
        wheel.navItems[2].navigateFunction = function () {
          self.clearOldContextMenu();
          self.editNode(nodeId);
        };

        // function 3: delete
        wheel.navItems[3].navigateFunction = function () {
          self.clearOldContextMenu();
          self.editNode(nodeId);
        };
      };

      $('#nodeContextMenu').css('left', x + 115);
      $('#nodeContextMenu').css('top', y + 72);
      $('#nodeContextMenu').width(100);
      $('#nodeContextMenu').height(100);

      generateMenu(e, nodeId);
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

      // create sigma graph
      var s = new this.Sigma({
        graph: graph,
        container: 'graph-container',
        renderer: {
          container: document.getElementById('graph-container'),
          type: 'canvas'
        },
        settings: {
          doubleClickEnabled: false,
          minEdgeSize: 0.5,
          maxEdgeSize: 4,
          enableEdgeHovering: true,
          // edgeHoverColor: 'edge',
          // defaultEdgeHoverColor: '#000',
          // defaultEdgeType: 'curve',
          edgeHoverSizeRatio: 1,
          edgeHoverExtremities: true
        }
      });

      sigma.plugins.fullScreen({
        container: 'graph-container',
        btnId: 'graph-fullscreen-btn'
      });

      var renderer = 'fruchtermann';

      if (renderer === 'noverlap') {
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
      } else if (renderer === 'fruchtermann') {
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

      if (document.addEventListener) {
        document.addEventListener('contextmenu', function (e) {
          // my custom functionality on right click
          e.preventDefault();
        }, false);
      } else {
        document.attachEvent('oncontextmenu', function () {
          // my custom functionality on right click
          window.event.returnValue = false;
        });
      }

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
        self.clearOldContextMenu();
      });

      var dragListener;
      // Initialize the dragNodes plugin:
      if (renderer === 'noverlap') {
        s.startNoverlap();
        // allow draggin nodes
        dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);
      } else if (renderer === 'force') {
        s.startForceAtlas2({worker: true, barnesHutOptimize: false});

        window.setTimeout(function () {
          s.stopForceAtlas2();
          dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);
          console.log('stopped force');
        }, 3000);
      } else if (renderer === 'fruchtermann') {
        // Start the Fruchterman-Reingold algorithm:
        sigma.layouts.fruchtermanReingold.start(s);
        dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);
      } else {
        dragListener = sigma.plugins.dragNodes(s, s.renderers[0]);
      }
      console.log(dragListener);
    }

  });
}());
