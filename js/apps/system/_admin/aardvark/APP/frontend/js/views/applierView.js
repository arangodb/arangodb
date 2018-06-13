/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, templateEngine, $, window, arangoHelper */
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

    breadcrumb: function (name) {
      var self = this;

      if (window.App.naviView) {
        var string = 'Database';
        if (name) {
          string = string + ': ' + name;
        }
        $('#subNavigationBar .breadcrumb').html(string);
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    },

    continueRender: function () {
      this.$el.html(this.template.render({}));
    },

    getApplierState: function (endpoint, global) {
      var self = this;
      var url;

      if (this.database === '_system' || global) {
        url = this.endpoint + '/_api/replication/applier-state';
      } else {
        url = this.endpoint + '/_db/' + this.database + '/_api/replication/applier-state';
      }
      if (global) {
        url = url + '?global=true';
        self.breadcrumb('All databases');
      } else {
        self.breadcrumb(self.database);
      }

      $.ajax({
        type: 'GET',
        cache: false,
        url: url,
        contentType: 'application/json',
        success: function (data) {
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
          if (data.mode === 3 || data.mode === 2) {
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
