/* jshint browser: true */
/* jshint unused: false */
/* global _, Backbone, templateEngine, $, window, arangoHelper */
(function () {
  'use strict';

  window.ApplierView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('applierView.ejs'),

    endpoint: null,

    events: {
    },

    render: function () {
      // mode found
      this.getMode();
    },

    continueRender: function () {
      this.$el.html(this.template.render({}));
    },

    getApplierState: function (endpoint, global) {
      var self = this;
      var url = this.endpoint + '/_api/replication/applier-state';
      if (global) {
        url = url + '?global=true';
      }

      $.ajax({
        type: 'GET',
        cache: false,
        url: url,
        contentType: 'application/json',
        success: function (data) {
          console.log(data);
          self.$el.html(self.template.render({
            data: data
          }));
        },
        error: function () {
          arangoHelper.arangoError('Replication', 'Could not fetch the applier state of: ' + endpoint);
        }
      });
    },

    getMode: function (callback) {
      var self = this;

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/aardvark/replication/mode'),
        contentType: 'application/json',
        success: function (data) {
          if (data.mode === 3) {
            self.getApplierState(this.endpoint, true);
          } else if (data.mode === 2) {
            self.getApplierState(this.endpoint, true);
          } else {
            self.getApplierState(this.endpoint, false);
          }
        },
        error: function () {
          arangoHelper.arangoError('Replication', 'Could not fetch the replication state.');
        }
      });
    }

  });
}());
