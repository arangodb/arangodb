/* global window, Backbone */
(function () {
  'use strict';

  window.ArangoMetricModel = Backbone.Model.extend({
    defaults: {
      name: '',
      type: '',
      info: '',
      metrics: []
    }

  });
}());
