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
        name: 'Starting node',
        desc: 'A valid node id. If empty, a random node will be chosen.',
        value: 2
      },
      'layout': {
        type: 'select',
        name: 'Layout algorithm',
        noverlap: {
          name: 'No overlap (fast)',
          val: 'noverlap'
        },
        force: {
          name: 'Force (slow)',
          val: 'force'
        },
        fruchtermann: {
          name: 'Fruchtermann (very slow)',
          val: 'fruchtermann'
        }
      },
      'renderer': {
        type: 'select',
        name: 'Renderer',
        canvas: {
          name: 'Canvas (editable)',
          val: 'canvas'
        },
        webgl: {
          name: 'WebGL (only display)',
          val: 'webgl'
        }
      },
      'depth': {
        type: 'number',
        name: 'Search depth',
        value: 2
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
        desc: 'Default node color. RGB or HEX value.',
        default: '_key'
      },
      'nodeColor': {
        type: 'color',
        name: 'Color',
        desc: 'Default node color. RGB or HEX value.',
        default: '#2ecc71'
      },
      'nodeSize': {
        type: 'string',
        name: 'Sizing attribute',
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
      'edgeColor': {
        type: 'color',
        name: 'Color',
        desc: 'Default edge color. RGB or HEX value.',
        default: '#cccccc'
      },
      'edgeSize': {
        type: 'number',
        name: 'Sizing',
        desc: 'Default edge thickness. Numeric value > 0.'
      },
      'edgeType': {
        type: 'select',
        name: 'Type',
        desc: 'The type of the edge',
        line: {
          name: 'Line',
          val: 'line'
        },
        curve: {
          name: 'Curve',
          val: 'curve'
        },
        arrow: {
          name: 'Arrow',
          val: 'arrow'
        },
        curvedArrow: {
          name: 'Curved Arrow',
          val: 'curvedArrow'
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
      'click #restoreGraphSettings': 'restoreGraphSettings',
      'keyup #graphSettingsView input': 'checkEnterKey',
      'keyup #graphSettingsView select': 'checkEnterKey',
      'focus #graphSettingsView input': 'lastFocus',
      'focus #graphSettingsView select': 'lastFocus'
    },

    lastFocus: function (e) {
      console.log(e.currentTarget.id);
      console.log(e.currentTarget);
      this.lastFocussed = e.currentTarget.id;
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

    saveGraphSettings: function () {
      var self = this;
      var combinedName = window.App.currentDB.toJSON().name + '_' + this.name;

      var config = {};
      config[combinedName] = {
        layout: $('#g_layout').val(),
        renderer: $('#g_renderer').val(),
        depth: $('#g_depth').val(),
        nodeColor: $('#g_nodeColor').val(),
        edgeColor: $('#g_edgeColor').val(),
        nodeLabel: $('#g_nodeLabel').val(),
        edgeLabel: $('#g_edgeLabel').val(),
        edgeType: $('#g_edgeType').val(),
        nodeSize: $('#g_nodeSize').val(),
        edgeSize: $('#g_edgeSize').val(),
        nodeStart: $('#g_nodeStart').val()
      };

      var callback = function () {
        if (window.App.graphViewer2) {
          window.App.graphViewer2.render(self.lastFocussed);
        } else {
          arangoHelper.arangoNotification('Graph ' + this.name, 'Configuration saved.');
        }
      }.bind(this);

      this.userConfig.setItem('graphs', config, callback);
    },

    setDefaults: function () {
      console.log('implement me!');
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
    },

    continueRender: function () {
      $(this.el).html(this.template.render({
        general: this.general,
        specific: this.specific
      }));

      if (this.graphConfig) {
        _.each(this.graphConfig, function (val, key) {
          $('#g_' + key).val(val);
        });
      } else {
        this.setDefaults();
      }

      // arangoHelper.buildGraphSubNav(this.name, 'Settings');

      // load graph settings from local storage
      // apply those values to view then
    }

  });
}());
