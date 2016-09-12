/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, _ */
(function () {
  'use strict';

  window.GraphSettingsView = Backbone.View.extend({
    el: '#graphSettingsContent',

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      return this;
    },

    general: {
      'graph': {
        type: 'divider',
        name: 'Graph'
      },
      'nodeStart': {
        type: 'string',
        name: 'Startnode',
        desc: 'A valid node id. If empty, a random node will be chosen.',
        value: 2
      },
      'layout': {
        type: 'select',
        name: 'Layout',
        desc: 'Different graph algorithms. No overlap is very fast (more than 5000 nodes), force is slower (less than 5000 nodes) and fruchtermann is the slowest (less than 500 nodes).',
        noverlap: {
          name: 'No overlap',
          val: 'noverlap'
        },
        force: {
          name: 'Force',
          val: 'force'
        },
        fruchtermann: {
          name: 'Fruchtermann',
          val: 'fruchtermann'
        }
      },
      'renderer': {
        type: 'select',
        name: 'Renderer',
        desc: 'Canvas enables editing, WebGL is only for displaying a graph but much faster.',
        canvas: {
          name: 'Canvas',
          val: 'canvas'
        },
        webgl: {
          name: 'WebGL',
          val: 'webgl'
        }
      },
      'depth': {
        desc: 'Search depth, starting from your start node.',
        type: 'number',
        name: 'Search Depth',
        value: 2
      },
      'limit': {
        desc: 'Limit nodes count. If empty or zero, no limit is set.',
        type: 'number',
        name: 'Limit',
        value: 250
      }
    },

    specific: {
      'nodes': {
        type: 'divider',
        name: 'Nodes'
      },
      'nodeLabel': {
        type: 'string',
        name: 'Label',
        desc: 'Node label. Please choose a valid and available node attribute.',
        default: '_key'
      },
      'nodeLabelByCollection': {
        type: 'select',
        name: 'Add Collection Name',
        desc: 'Append collection name to the label?',
        yes: {
          name: 'Yes',
          val: 'true'
        },
        no: {
          name: 'No',
          val: 'false'
        }
      },
      'nodeColorByCollection': {
        type: 'select',
        name: 'Color By Collections',
        no: {
          name: 'No',
          val: 'false'
        },
        yes: {
          name: 'Yes',
          val: 'true'
        },
        desc: 'Should nodes be colorized by their collection? If enabled, node color and node color attribute will be ignored.'
      },
      'nodeColor': {
        type: 'color',
        name: 'Color',
        desc: 'Default node color. RGB or HEX value.',
        default: '#2ecc71'
      },
      'nodeColorAttribute': {
        type: 'string',
        name: 'Color Attribute',
        desc: 'If an attribute is given, nodes will then be colorized by the attribute. This setting ignores default node color if set.'
      },
      'nodeSizeByEdges': {
        type: 'select',
        name: 'Size By Collections',
        yes: {
          name: 'Yes',
          val: 'true'
        },
        no: {
          name: 'No',
          val: 'false'
        },
        desc: 'Should nodes be sized by their edges count? If enabled, node sizing attribute will be ignored.'
      },
      'nodeSize': {
        type: 'string',
        name: 'Sizing Attribute',
        desc: 'Default node size. Numeric value > 0.'
      },
      'edges': {
        type: 'divider',
        name: 'Edges'
      },
      'edgeLabel': {
        type: 'string',
        name: 'Label',
        desc: 'Default edge label.'
      },
      'edgeLabelByCollection': {
        type: 'select',
        name: 'Add Collection Name',
        desc: 'Set label text by collection. If activated edge label attribute will be ignored.',
        yes: {
          name: 'Yes',
          val: 'true'
        },
        no: {
          name: 'No',
          val: 'false'
        }
      },
      'edgeColorByCollection': {
        type: 'select',
        name: 'Color By Collections',
        no: {
          name: 'No',
          val: 'false'
        },
        yes: {
          name: 'Yes',
          val: 'true'
        },
        desc: 'Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored.'
      },
      'edgeColor': {
        type: 'color',
        name: 'Color',
        desc: 'Default edge color. RGB or HEX value.',
        default: '#cccccc'
      },
      'edgeColorAttribute': {
        type: 'string',
        name: 'Color Attribute',
        desc: 'If an attribute is given, edges will then be colorized by the attribute. This setting ignores default edge color if set.'
      },
      'edgeEditable': {
        type: 'select',
        hide: 'true',
        name: 'Editable',
        yes: {
          name: 'Yes',
          val: 'true'
        },
        no: {
          name: 'No',
          val: 'false'
        },
        desc: 'Should edges be editable?'
      },
      'edgeType': {
        type: 'select',
        name: 'Type',
        desc: 'The type of the edge',
        line: {
          name: 'Line',
          val: 'line'
        },
        arrow: {
          name: 'Arrow',
          val: 'arrow'
        },
        curve: {
          name: 'Curve',
          val: 'curve'
        },
        dotted: {
          name: 'Dotted',
          val: 'dotted'
        },
        dashed: {
          name: 'Dashed',
          val: 'dashed'
        },
        tapered: {
          name: 'Tapered',
          val: 'tapered'
        }
      }
    },

    template: templateEngine.createTemplate('graphSettingsView.ejs'),

    initialize: function (options) {
      this.name = options.name;
      this.userConfig = options.userConfig;
      this.saveCallback = options.saveCallback;
    },

    events: {
      'click #saveGraphSettings': 'saveGraphSettings',
      'click #restoreGraphSettings': 'setDefaults',
      'keyup #graphSettingsView input': 'checkEnterKey',
      'keyup #graphSettingsView select': 'checkEnterKey',
      'change input[type="range"]': 'saveGraphSettings',
      'change input[type="color"]': 'checkColor',
      'change select': 'saveGraphSettings',
      'focus #graphSettingsView input': 'lastFocus',
      'focus #graphSettingsView select': 'lastFocus',
      'focusout #graphSettingsView input[type="text"]': 'checkinput'
    },

    lastFocus: function (e) {
      this.lastFocussed = e.currentTarget.id;
      this.lastFocussedValue = $(e.currentTarget).val();
    },

    checkinput: function (e) {
      if ((new Date() - this.lastSaved > 500)) {
        if (e.currentTarget.id === this.lastFocussed) {
          if (this.lastFocussedValue !== $(e.currentTarget).val()) {
            this.saveGraphSettings();
          }
        }
      }
    },

    checkEnterKey: function (e) {
      if (e.keyCode === 13) {
        this.saveGraphSettings();
      }
    },

    getGraphSettings: function (render) {
      var self = this;
      var combinedName = window.App.currentDB.toJSON().name + '_' + this.name;

      this.userConfig.fetch({
        success: function (data) {
          self.graphConfig = data.toJSON().graphs[combinedName];
          if (render) {
            self.continueRender();
          }
        }
      });
    },

    checkColor: function () {
      this.saveGraphSettings(null, true);
    },

    saveGraphSettings: function (event, color, nodeStart, overwrite) {
      var self = this;
      self.lastSaved = new Date();
      var combinedName = window.App.currentDB.toJSON().name + '_' + this.name;

      var config = {};

      if (overwrite) {
        config[combinedName] = overwrite;
      } else {
        var object = {};

        var id;
        $('#graphSettingsView select').each(function (key, elem) {
          id = elem.id;
          object[id.substr(2, elem.id.length)] = $(elem).val();
        });
        $('#graphSettingsView input').each(function (key, elem) {
          id = elem.id;
          object[id.substr(2, elem.id.length)] = $(elem).val();
        });

        config[combinedName] = object;
      }

      if (nodeStart) {
        config[combinedName].nodeStart = nodeStart;
      }

      var callback = function () {
        if (window.App.graphViewer) {
          if (color !== '' && color !== undefined) {
            var nodes = !$('#g_nodeColor').is(':disabled');
            var edges = !$('#g_edgeColor').is(':disabled');
            window.App.graphViewer.updateColors(
              nodes,
              edges,
              $('#g_nodeColor').val(),
              $('#g_edgeColor').val()
            );
          } else {
            window.App.graphViewer.render(self.lastFocussed);
          }
        } else {
          arangoHelper.arangoNotification('Graph ' + this.name, 'Configuration saved.');
        }
      }.bind(this);

      this.userConfig.setItem('graphs', config, callback);
    },

    setDefaults: function (onlySave) {
      var obj = {
        layout: 'force',
        renderer: 'canvas',
        depth: '2',
        limit: '250',
        nodeColor: '#2ecc71',
        nodeColorAttribute: '',
        nodeColorByCollection: 'true',
        edgeColor: '#cccccc',
        edgeColorAttribute: '',
        edgeColorByCollection: 'true',
        nodeLabel: '_key',
        edgeLabel: '',
        edgeType: 'arrow',
        nodeSize: '',
        nodeSizeByEdges: 'true',
        edgeEditable: 'true',
        nodeLabelByCollection: 'false',
        edgeLabelByCollection: 'false',
        nodeStart: '',
        barnesHutOptimize: true
      };

      if (onlySave === true) {
        this.saveGraphSettings(null, null, null, obj);
      } else {
        this.saveGraphSettings(null, null, null, obj);
        this.render();
        window.App.graphViewer.render(this.lastFocussed);
      }
    },

    toggle: function () {
      if ($(this.el).is(':visible')) {
        this.hide();
      } else {
        this.show();
      }
    },

    show: function () {
      $(this.el).show('slide', {direction: 'right'}, 250);
    },

    hide: function () {
      $(this.el).hide('slide', {direction: 'right'}, 250);
    },

    render: function () {
      this.getGraphSettings(true);
      this.lastSaved = new Date();
    },

    handleDependencies: function () {
      // node sizing
      if ($('#g_nodeSizeByEdges').val() === 'true') {
        $('#g_nodeSize').prop('disabled', true);
      }

      // node color
      if ($('#g_nodeColorByCollection').val() === 'true') {
        $('#g_nodeColorAttribute').prop('disabled', true);
        $('#g_nodeColor').prop('disabled', true);
      }
      if ($('#g_nodeColorAttribute').val() !== '') {
        $('#g_nodeColor').prop('disabled', true);
      }

      // edge color
      if ($('#g_edgeColorByCollection').val() === 'true') {
        $('#g_edgeColorAttribute').prop('disabled', true);
        $('#g_edgeColor').prop('disabled', true);
      }
      if ($('#g_edgeColorAttribute').val() !== '') {
        $('#g_edgeColor').prop('disabled', true);
      }
    },

    continueRender: function () {
      $(this.el).html(this.template.render({
        general: this.general,
        specific: this.specific
      }));

      arangoHelper.fixTooltips('.gv-tooltips', 'top');

      if (this.graphConfig) {
        _.each(this.graphConfig, function (val, key) {
          $('#g_' + key).val(val);
        });
      } else {
        this.setDefaults(true);
      }

      this.handleDependencies();
    }

  });
}());
