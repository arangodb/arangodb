/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true, stupid: true*/
/*global require, exports, Backbone, activeUser, window, ArangoQuery, $, data, _ */
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

    saveImportQueries: function(file) {

      if (this.activeUser === 0) {
        return false;
      }

      var queries = [];
      this.each(function(query) {
        queries.push({
          value: query.attributes.value,
          name: query.attributes.name
        });
      });

      var self = this,
      returnValue1 = false,
      returnValue2 = false,
      returnValue3 = false;

      var oldQueries = this.currentExtra;

      $.ajax({
        cache: false,
        type: "PATCH",
        async: false,
        url: "/_api/user/" + this.activeUser,
        data: file,
        contentType: "application/json",
        processData: false,
        success: function() {
          returnValue1 = true;
        },
        error: function() {
          returnValue1 = false;
        }
      });

      this.fetch({
        async: false,
        success: function() {
      }});

      var newQueries = self.currentExtra;

      var nameTaken = false;

      _.each(oldQueries.queries, function(query) {

        _.each(newQueries.queries, function(newQuery) {
          if (newQuery.name === query.name) {
            nameTaken = true;
          }
        });

        if (nameTaken === false) {
          newQueries.queries.push({
            name: query.name,
            value: query.value
          });
        }
      });

      $.ajax({
        cache: false,
        type: "PATCH",
        async: false,
        url: "/_api/user/" + this.activeUser,
        data: JSON.stringify({
          extra: newQueries
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

      this.fetch({
        async: false
      });

      if (returnValue1 === true && returnValue2 === true) {
        returnValue3 = true;
      }
      else {
        returnValue3 = false;
      }
      return returnValue3;
    }

  });
}());
