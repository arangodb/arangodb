/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true, stupid: true*/
/*global require, exports, Backbone, window, arangoCollectionModel, $, arangoHelper, data, _ */
(function() {
  "use strict";

  window.ArangoQueries = Backbone.Collection.extend({

    initialize: function(models, options) {
      this.fetch();

      options || (options = {});
      if (options.activeUser) {
        this.activeUser = options.activeUser;
      }
      else {
        this.activeUser = "root";
      }
    },

    url: '/_api/user',

    model: ArangoQuery,

    activeUser: 0,

    parse: function(response)  {
      var self = this, toReturn;

      _.each(response.result, function(val) {
        if (val.user === self.activeUser) {
          try {
            if (val.extra.queries) {
              toReturn = val.extra.queries;
            }
          }
          catch (e) {
          }
        }
      });
      return toReturn;
    }

  });
}());
