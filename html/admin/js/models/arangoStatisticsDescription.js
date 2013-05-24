/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, nonpropdel: true, continue: true, regexp: true */
/*global require, window, Backbone */

window.StatisticsDescription = Backbone.Model.extend({
  defaults: {
    "figures" : "",
    "groups" : ""
  },
  
  url: function() {
    'use strict';

    return "../statistics-description";
  }  
});
