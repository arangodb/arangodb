/* jshint browser: true */
/* jshint unused: false */
/* global window, _, arangoHelper, Backbone */

(function () {
  'use strict';

  window.ArangoMetrics = Backbone.Collection.extend({
    txt: {},

    // call original Backbone.Model#fetch with `dataType` equal `text` for $.ajax
    fetch: function (options) {
      options = _.extend(options || {}, {
        dataType: 'text'
      });
      this.constructor.__super__.fetch.call(this, options);
    },

    metricsAsText: function () {
      return this.txt;
    },

    comparator: function(metric) {
      return metric.get('name');
    },

    // store response in content attribute
    parse: function (response) {
      let models = [];
      // store original response to provide download later
      this.txt = response;
      let json = window.parsePrometheusTextFormat(response);

      _.each(json, (entry) => {
        models.push({
            name: entry.name,
            type: entry.type,
            info: entry.help,
            metrics: entry.metrics
          });
      });
      return models;
    },

    initialize: function (options) {
      if (options.endpoint) {
        this.endpoint = options.endpoint;
      }
    },

    model: window.ArangoMetricModel,

    url: function () {
      if (this.endpoint) {
        return arangoHelper.databaseUrl('/_admin/metrics/v2?serverId=' + encodeURIComponent(this.endpoint));
      }
      return arangoHelper.databaseUrl('/_admin/metrics/v2');
    }

  });
}());
