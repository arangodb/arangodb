/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, templateEngine, arangoHelper, $, window, frontendConfig */
(function () {
  'use strict';

  window.MetricsView = Backbone.View.extend({
    template: templateEngine.createTemplate('metricsView.ejs'),
    infoTemplate: templateEngine.createTemplate('infoView.ejs'),
    el: '#content',
    metricsel: undefined,
    interval: 10000,
    activeView: 'table',

    events: {
      'click #toggleView': "toggleView",
      'click #downloadAs': "downloadAs",
      'click #reloadMetrics': "reloadMetrics"
    },

    initialize: function (options) {
      if (options) {
        this.options = options;

        if (options.endpoint) {
          this.endpoint = options.endpoint;
        }
        if (options.contentDiv) {
          this.metricsel = options.contentDiv;
        }
      }
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
      if (this.activeView === 'table') {
        $('#toggleView').text('Show Table');
        $('#metricsAsText').show();
        $('#metricsAsTable').hide();
        this.activeView = 'text';
      } else {
        $('#toggleView').text('Show Text');
        $('#metricsAsText').hide();
        $('#metricsAsTable').show();
        this.activeView = 'table';
      }
    },

    breadcrumb: function (name) {
      $('#subNavigationBar .breadcrumb').html('Metrics');
    },

    reloadMetrics: function () {
      let self = this;
      this.collection.fetch({
        success: function () {
          arangoHelper.arangoNotification('Metrics', 'Reloaded metrics.');
          self.continueRender();
        }
      });
    },

    render: function () {
      var self = this;

      if (!frontendConfig.metricsEnabled) {
        let disabled = {
          title: "Metrics",
          message: "Metrics are disabled."
        };

        if (this.metricsel) {
          $(this.metricsel).html(this.infoTemplate.render(disabled));
        } else {
          this.$el.html(this.infoTemplate.render(disabled));
        }
      } else {
        this.collection.fetch({
          success: function () {
            self.continueRender();
          }
        });
      }
    },

    continueRender: function () {
      if (this.metricsel) {
        $(this.metricsel).html(this.template.render({
          collection: this.collection,
          activeView: this.activeView
        }));
      } else {
        this.$el.html(this.template.render({
          collection: this.collection,
          activeView: this.activeView
        }));
      }

    }
  });
}());
