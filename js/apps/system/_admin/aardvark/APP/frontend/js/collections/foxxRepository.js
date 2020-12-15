/* jshint browser: true */
/* jshint unused: false */
/* global window, _, Backbone, arangoHelper */
(function () {
  'use strict';
  window.FoxxRepository = Backbone.Collection.extend({
    model: window.FoxxRepoModel,

    sortOptions: {
      desc: false
    },

    url: arangoHelper.databaseUrl('/_admin/aardvark/foxxes/fishbowl'),

    parse: function (response) {
      var foxxes = [];
      _.each(response, function (foxx) {
        // hide legacy applications
        if (!foxx.legacy) {
          foxxes.push(foxx);
        }
        // }
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
