/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true, stupid: true*/
/*global require, exports, Backbone, window, ArangoQuery, $, data, _ */
(function() {
  "use strict";

  window.ArangoQueries = Backbone.Collection.extend({

    initialize: function(models, options) {
      this.fetch();

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

    parse: function(response) {
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
    },

    saveQueries: function(data) {
      var returnValue = false;

      $.ajax({
        cache: false,
        type: "PATCH",
        async: false,
        url: "/_api/user/" + this.activeUser,
        data: {
          extra: {
            queries: data
          }
        },
        contentType: "application/json",
        processData: false,
        success: function() {
          returnValue = true;
        },
        error: function() {
          returnValue = false;
        }
      });

      return returnValue;
    }

  });
}());
