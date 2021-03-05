/* jshint browser: true */
/* jshint unused: false */
/* global window, _, arangoHelper */

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

    // store response in content attribute
    parse: function (response) {
      let models = [];
      // store original response to provide download later
      this.txt = response;
      let json = window.parsePrometheusTextFormat(response);

      let currentType;
      let currentName;
      let currentInfo;
      let currentData = [];

      for (let i = 0; i < json.length; i++) {
        if (json[i].name === '#TYPE') {
          if (i > 0) {
            // push to models
            models.push({
              name: currentName,
              type: currentType,
              info: currentInfo,
              metrics: currentData
            });

            // clear states
            currentType = '';
            currentName = '';
            currentInfo = '';
            currentData = [];
          }

          // found type definition
          currentName = json[i].metrics[0].value;
          currentType = json[i].metrics[0].timestamp_ms;
        } else if (json[i].name === '#HELP') {
          // found help definition
          currentInfo = json[i].metrics[0].timestamp_ms;
        } else {
          // found data entry
          _.each(json[i].metrics, (metric) => {
            currentData.push(metric);
          });
        }
      }
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
        return arangoHelper.databaseUrl('/_admin/metrics?server=' + encodeURIComponent(this.endpoint));
      }
      return arangoHelper.databaseUrl('/_admin/metrics');
    }

  });
}());
