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

    currentExtra: {},

    parse: function(response) {
      var self = this, toReturn;

      _.each(response.result, function(val) {
        if (val.user === self.activeUser) {
          self.currentExtra = val.extra;
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

      var queries = [],
      returnValue1 = false,
      returnValue2 = false,
      returnValue3 = false,
      extraBackup = null,
      self = this;

      this.each(function(query) {
        queries.push({
          value: query.attributes.value,
          name: query.attributes.name
        });
      });

      extraBackup = self.currentExtra;
      extraBackup.queries = [];

      $.ajax({
        cache: false,
        type: "PUT",
        async: false,
        url: "/_api/user/" + this.activeUser,
        data: JSON.stringify({
          extra: extraBackup
        }),
        contentType: "application/json",
        processData: false,
        success: function() {
          returnValue1 = true;
        },
        error: function() {
          returnValue1 = false;
        }
      });

      //save current collection
      $.ajax({
        cache: false,
        type: "PATCH",
        async: false,
        url: "/_api/user/" + this.activeUser,
        data: JSON.stringify({
          extra: {
           queries: queries
          }
        }),
        contentType: "application/json",
        processData: false,
        success: function() {
          returnValue2 = true;
        },
        error: function() {
          returnValue2 = false;
        }
      });

      if (returnValue1 === true && returnValue2 === true) {
        returnValue3 = true;
      }
      else {
        returnValue3 = false;
      }
      return returnValue3;
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
        url: "query/upload/" + this.activeUser,
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
