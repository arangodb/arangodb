/* jshint browser: true */
/* jshint unused: false */
/* global window, _, Backbone */
(function () {
  'use strict';
  window.FoxxRepository = Backbone.Collection.extend({
    model: window.FoxxRepoModel,

    sortOptions: {
      desc: false
    },

    url: 'foxxes/fishbowl',

    parse: function (response) {
      var foxxes = [];
      var version;
      _.each(response, function (foxx) {
        version = Number.parseInt(foxx.latestVersion.charAt(0));
        if (version >= 3) {
          foxxes.push(foxx);
        }
      });
      return foxxes;
    },

    comparator: function (item, item2) {
      var a, b;
      if (this.sortOptions.desc === true) {
        a = item.get('mount');
        b = item2.get('mount');
        return a < b ? 1 : a > b ? -1 : 0;
      }
      a = item.get('mount');
      b = item2.get('mount');
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function (val) {
      this.sortOptions.desc = val;
    }

  });
}());
