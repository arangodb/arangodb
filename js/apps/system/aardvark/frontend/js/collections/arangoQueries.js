/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, activeUser, window, ArangoQuery, $, data, _, arangoHelper*/
(function() {
  "use strict";

  window.ArangoQueries = Backbone.Collection.extend({

    initialize: function(models, options) {
      var result;
      $.ajax("whoAmI", {async:false}).done(
        function(data) {
        result = data.name;
      }
      );
      this.activeUser = result;

      if (this.activeUser === 0 || this.activeUser === undefined || this.activeUser === null) {
        this.activeUser = "root";
      }

    },

    url: '/_api/user/',

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

    saveCollectionQueries: function() {
      if (this.activeUser === 0) {
        return false;
      }

      var returnValue = false;
      var queries = [];

      this.each(function(query) {
        queries.push({
          value: query.attributes.value,
          name: query.attributes.name
        });
      });

      // save current collection
      $.ajax({
        cache: false,
        type: "PATCH",
        async: false,
        url: "/_api/user/" + encodeURIComponent(this.activeUser),
        data: JSON.stringify({
          extra: {
           queries: queries
          }
        }),
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
    },

    saveImportQueries: function(file, callback) {

      if (this.activeUser === 0) {
        return false;
      }

      window.progressView.show("Fetching documents...");
      $.ajax({
        cache: false,
        type: "POST",
        async: false,
        url: "query/upload/" + encodeURIComponent(this.activeUser),
        data: file,
        contentType: "application/json",
        processData: false,
        success: function() {
          window.progressView.hide();
          callback();
        },
        error: function() {
          window.progressView.hide();
          arangoHelper.arangoError("Query error", "queries could not be imported");
        }
      });
    }

  });
}());
