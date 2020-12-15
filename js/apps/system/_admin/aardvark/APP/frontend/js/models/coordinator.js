/* global window, Backbone */
(function () {
  'use strict';

  window.Coordinator = Backbone.Model.extend({
    defaults: {
      address: '',
      protocol: '',
      name: '',
      status: ''
    }

  });
}());
