/* global window, Backbone */
(function () {
  'use strict';

  window.workMonitorModel = Backbone.Model.extend({
    defaults: {
      name: '',
      number: '',
      status: '',
      type: ''
    }

  });
}());
