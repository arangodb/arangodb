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

    comparator: function(item, item2) {
      var a = item.get('_key') || "";
      var b = item2.get('_key') || "";
      a = a.toLowerCase();
      b = b.toLowerCase();
      if (this.sortOptions.desc === true) {
        return a < b ? 1 : a > b ? -1 : 0;
      }
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
