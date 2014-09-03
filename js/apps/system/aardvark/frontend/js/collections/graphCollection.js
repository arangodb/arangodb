/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone */
(function () {
  "use strict";

  window.GraphCollection = Backbone.Collection.extend({
    model: window.Graph,

    sortOptions: {
      desc: false
    },

    url: "/_api/gharial",

    //comparator: "_key",

    comparator: function(item, item2) {
      var a, b;
      if (this.sortOptions.desc === true) {
        a = item.get('_key');
        b = item2.get('_key');
        return a < b ? 1 : a > b ? -1 : 0;
      }
      a = item.get('_key');
      b = item2.get('_key');
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function(val) {
      this.sortOptions.desc = val;
    },

    parse: function(res) {
      if (!res.error) {
        return res.graphs;
      }
    }
  });
}());
