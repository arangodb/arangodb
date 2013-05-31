/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone*/

window.Statistics = Backbone.Model.extend({
  defaults: {
  },
  url: function() {
    return "../statistics";
  }
});
