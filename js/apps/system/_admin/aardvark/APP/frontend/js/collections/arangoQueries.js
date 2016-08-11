/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, window, ArangoQuery, $, _, arangoHelper*/
(function () {
  'use strict';

  window.ArangoQueries = Backbone.Collection.extend({
    initialize: function (models, options) {
      var self = this;

      $.ajax('whoAmI?_=' + Date.now(), {async: true}).done(
        function (data) {
          if (this.activeUser === false || this.activeUser === null) {
            self.activeUser = 'root';
          } else {
            self.activeUser = data.user;
          }
        }
      );
    },

    url: arangoHelper.databaseUrl('/_api/user/'),

    model: ArangoQuery,

    activeUser: null,

    parse: function (response) {
      var self = this; var toReturn;
      if (this.activeUser === false || this.activeUser === null) {
        this.activeUser = 'root';
      }

      _.each(response.result, function (val) {
        if (val.user === self.activeUser) {
          try {
            if (val.extra.queries) {
              toReturn = val.extra.queries;
            }
          } catch (e) {}
        }
      });
      return toReturn;
    },

    saveCollectionQueries: function (callback) {
      if (this.activeUser === false || this.activeUser === null) {
        this.activeUser = 'root';
      }

      var queries = [];

      this.each(function (query) {
        queries.push({
          value: query.attributes.value,
          parameter: query.attributes.parameter,
          name: query.attributes.name
        });
      });

      // save current collection
      $.ajax({
        cache: false,
        type: 'PATCH',
        url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.activeUser)),
        data: JSON.stringify({
          extra: {
            queries: queries
          }
        }),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data);
        },
        error: function () {
          callback(true);
        }
      });
    },

    saveImportQueries: function (file, callback) {
      if (this.activeUser === 0) {
        return false;
      }

      window.progressView.show('Fetching documents...');
      $.ajax({
        cache: false,
        type: 'POST',
        url: 'query/upload/' + encodeURIComponent(this.activeUser),
        data: file,
        contentType: 'application/json',
        processData: false,
        success: function () {
          window.progressView.hide();
          arangoHelper.arangoNotification('Queries successfully imported.');
          callback();
        },
        error: function () {
          window.progressView.hide();
          arangoHelper.arangoError('Query error', 'queries could not be imported');
        }
      });
    }

  });
}());
