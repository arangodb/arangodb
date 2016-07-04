/* jshint browser: true, evil: true */
/* jshint unused: false */
/* global Backbone, templateEngine, $, window*/

(function () {
  'use strict';

  window.EditListEntryView = Backbone.View.extend({
    template: templateEngine.createTemplate('editListEntryView.ejs'),

    initialize: function (opts) {
      this.key = opts.key;
      this.value = opts.value;
      this.render();
    },

    events: {
      'click .deleteAttribute': 'removeRow'
    },

    render: function () {
      $(this.el).html(this.template.render({
        key: this.key,
        value: JSON.stringify(this.value),
        isReadOnly: this.isReadOnly()
      }));
    },

    isReadOnly: function () {
      return this.key.indexOf('_') === 0;
    },

    getKey: function () {
      return $('.key').val();
    },

    getValue: function () {
      var val = $('.val').val();
      try {
        val = JSON.parse(val);
      } catch (e) {
        try {
          eval('val = ' + val);
          return val;
        } catch (e2) {
          return $('.val').val();
        }
      }
      return val;
    },

    removeRow: function () {
      this.remove();
    }
  });
}());
