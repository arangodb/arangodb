/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window*/
(function () {
  'use strict';

  window.GraphSettingsView = Backbone.View.extend({
    el: '#content',

    general: {
      'layout': undefined,
      'depth': undefined
    },

    specific: {
      'node_label': undefined,
      'node_color': undefined,
      'node_size': undefined
    },

    template: templateEngine.createTemplate('graphSettingsView.ejs'),

    initialize: function (options) {
      this.name = options.name;
    },

    events: {
    },

    render: function () {
      $(this.el).html(this.template.render({
        general: this.general,
        specific: this.specific
      }));
      arangoHelper.buildGraphSubNav(this.name, 'Settings');
    }

  });
}());
