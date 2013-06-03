/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, continue: true, regexp: true */
/*global require, window, Backbone */

window.Statistics = Backbone.Model.extend({
  defaults: {
  },
  url: function() {
    'use strict';

    return "../statistics";
  }
});
