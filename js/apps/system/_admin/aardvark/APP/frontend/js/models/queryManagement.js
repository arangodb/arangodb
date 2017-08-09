/* global window, Backbone */
(function () {
  'use strict';

  window.queryManagementModel = Backbone.Model.extend({
    defaults: {
      id: '',
      query: '',
      bindVars: '',
      started: '',
      runTime: ''
    }

  });
}());
