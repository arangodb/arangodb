/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, _, frontendConfig, window, ArangoQuery, $, arangoHelper */
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

    fetch: function (options) {
      if (window.App.currentUser && window.App.currentDB.get('name') !== '_system') {
        this.url = frontendConfig.basePath + '/_api/user/' + encodeURIComponent(window.App.currentUser);
      }
      return Backbone.Collection.prototype.fetch.call(this, options);
    },

    url: arangoHelper.databaseUrl('/_api/user/'),

    model: ArangoQuery,

    activeUser: null,

    parse: function (response) {
      var self = this; var toReturn;
      if (this.activeUser === false || this.activeUser === null) {
        this.activeUser = 'root';
      }

      if (response.user === self.activeUser) {
        try {
          if (response.extra.queries) {
            toReturn = response.extra.queries;
          }
        } catch (e) {}
      } else {
        if (response.result && response.result instanceof Array) {
          _.each(response.result, function (userobj) {
            if (userobj.user === self.activeUser) {
              toReturn = userobj.extra.queries;
            }
          });
        }
      }

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
