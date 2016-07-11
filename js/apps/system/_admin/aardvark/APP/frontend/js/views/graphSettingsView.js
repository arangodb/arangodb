/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window*/
(function () {
  'use strict';

  window.GraphSettingsView = Backbone.View.extend({
    el: '#content',

    general: {
      'Layout': {
        type: 'select',
        noverlap: {
          name: 'No overlap (fast)'
        },
        force: {
          name: 'Force (slow)'
        },
        fruchtermann: {
          name: 'Fruchtermann (very slow)'
        }
      },
      'Renderer': {
        type: 'select',
        canvas: {
          name: 'Canvas (editable)'
        },
        webgl: {
          name: 'WebGL (only display)'
        }
      },
      'depth': {
        type: 'numeric',
        value: 2
      }
    },

    specific: {
      'nodeLabel': {
        type: 'string',
        name: 'Node label',
        desc: 'Default node color. RGB or HEX value.',
        default: '_key'
      },
      'nodeColor': {
        type: 'color',
        name: 'Node color',
        desc: 'Default node color. RGB or HEX value.',
        default: '#2ecc71'
      },
      'nodeSize': {
        type: 'string',
        name: 'Node size',
        desc: 'Default node size. Numeric value > 0.',
        value: undefined
      },
      'edgeLabel': {
        type: 'string',
        name: 'Edge label',
        desc: 'Default edge label.',
        value: undefined
      },
      'edgeColor': {
        type: 'color',
        name: 'Edge color',
        desc: 'Default edge color. RGB or HEX value.',
        default: '#cccccc'
      },
      'edgeSize': {
        type: 'string',
        name: 'Edge thickness',
        desc: 'Default edge thickness. Numeric value > 0.',
        value: undefined
      },
      'edgeType': {
        type: 'select',
        name: 'Edge type',
        desc: 'The type of the edge',
        canvas: {
          name: 'Straight'
        },
        webgl: {
          name: 'Curved'
        }
      }
    },

    template: templateEngine.createTemplate('graphSettingsView.ejs'),

    initialize: function (options) {
      this.name = options.name;
    },

    loadGraphSettings: function () {

    },

    saveGraphSettings: function () {

    },

    events: {
    },

    render: function () {
      $(this.el).html(this.template.render({
        general: this.general,
        specific: this.specific
      }));
      arangoHelper.buildGraphSubNav(this.name, 'Settings');

      // load graph settings from local storage
      // apply those values to view then
    }

  });
}());
