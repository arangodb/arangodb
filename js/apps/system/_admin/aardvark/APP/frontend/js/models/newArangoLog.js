/* global window, Backbone */
(function () {
  'use strict';

  window.newArangoLog = Backbone.Model.extend({
    defaults: {
      lid: '',
      level: '',
      timestamp: '',
      text: '',
      totalAmount: ''
    },

    getLogStatus: function () {
      switch (this.get('level')) {
        case 1:
          return 'Error';
        case 2:
          return 'Warning';
        case 3:
          return 'Info';
        case 4:
          return 'Debug';
        default:
          return 'Unknown';
      }
    }
  });
}());
