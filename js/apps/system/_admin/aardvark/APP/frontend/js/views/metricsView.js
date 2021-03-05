/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, templateEngine, arangoHelper, $, window */
(function () {
  'use strict';

  window.MetricsView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('metricsView.ejs'),
    interval: 10000,
    activeView: 'table',

    events: {
      'click #toggleView': "toggleView",
      'click #downloadAs': "downloadAs"
    },

    initialize: function (options) {
      // start polling with interval
      window.setInterval(function () {
        if (window.location.hash === '#metrics') {
        }
      }, this.interval);
    },

    downloadAs: function () {
      let currentDate = new Date();
      let fileName = `METRICS-${currentDate.toISOString()}`;
      if (this.activeView === 'table') {
        arangoHelper.downloadLocalBlob(JSON.stringify(this.collection.toJSON(), null, 2), 'json', fileName);
      } else {
        arangoHelper.downloadLocalBlob(this.collection.metricsAsText(), 'text', fileName);
      }
    },

    toggleView: function () {
      console.log("click");
      if (this.activeView === 'table') {
        $('#toggleView').text('Show as table');
        $('#metricsAsText').show();
        $('#metricsAsTable').hide();
        this.activeView = 'text';
      } else {
        $('#toggleView').text('Show as text');
        $('#metricsAsText').hide();
        $('#metricsAsTable').show();
        this.activeView = 'table';
      }
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    breadcrumb: function (name) {
      $('#subNavigationBar .breadcrumb').html('Metrics');
    },

    render: function () {
      var self = this;

      this.collection.fetch({
        success: function () {
          self.continueRender();
        }
      });

      this.$el.html(this.template.render({
        collection: this.collection
      }));

      var metricsCallback = function () {
        this.continueRender();
      }.bind(this);
    },

    continueRender: function () {
      this.$el.html(this.template.render({
        collection: this.collection
      }));
    }

  });
}());
