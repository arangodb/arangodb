/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, _, FileReader, sessionStorage, frontendConfig, window, ArangoQuery, $, arangoHelper */
(function () {
  'use strict';

  window.ArangoQueries = Backbone.Collection.extend({

    getQueryPath: function () {
      return frontendConfig.db + '-' + this.username + '-queries';
    },

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
      options = _.extend({parse: true}, options);
      var model = this;
      var success = options.success;

      if (frontendConfig.ldapEnabled) {
        this.fetchLocalQueries();
        options.success = (function (resp) {
          // if success function available, call it
          if (success) {
            success.call(options.context, model, resp, options);
          }
        })();
      } else {
        if (frontendConfig.authenticationEnabled && window.App.currentUser) {
          this.url = arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(window.App.currentUser));
        } else {
          this.url = arangoHelper.databaseUrl('/_api/user/');
        }
        return Backbone.Collection.prototype.fetch.call(this, options);
      }
    },

    fetchLocalQueries: function () {
      // remove local available queries
      this.reset();
      var self = this;
      // fetch and add queries
      var item = sessionStorage.getItem(this.getQueryPath());
      try {
        item = JSON.parse(item);
        _.each(item, function (val, key) {
          self.add(val);
        });
      } catch (ignore) {
      }
    },

    url: arangoHelper.databaseUrl('/_api/user/'),

    model: ArangoQuery,

    activeUser: null,

    parse: function (response) {
      var self = this; var toReturn;
      if (this.activeUser === false || this.activeUser === null) {
        this.activeUser = 'root';
      }

      if (response.user === window.App.currentUser) {
        try {
          if (response.extra.queries) {
            toReturn = response.extra.queries;
          }
        } catch (e) {}
      } else {
        if (response.result && response.result instanceof Array) {
          _.each(response.result, function (userobj) {
            if (userobj.user === window.App.currentUser) {
              toReturn = userobj.extra.queries;
            }
          });
        }
      }

      return toReturn;
    },

    saveLocalCollectionQueries: function (data, callbackFunc) {
      sessionStorage.setItem(this.getQueryPath(), JSON.stringify(data));

      if (callbackFunc) {
        callbackFunc(false, data);
      }
    },

    saveCollectionQueries: function (callbackFunc) {
      var queries = [];

      this.each(function (query) {
        queries.push({
          value: query.attributes.value,
          parameter: query.attributes.parameter,
          name: query.attributes.name
        });
      });

      if (frontendConfig.ldapEnabled) {
        this.saveLocalCollectionQueries(queries, callbackFunc);
      } else {
        if (this.activeUser === false || this.activeUser === null) {
          this.activeUser = 'root';
        }

        // save current collection
        $.ajax({
          cache: false,
          type: 'PATCH',
          url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(window.App.currentUser)),
          data: JSON.stringify({
            extra: {
              queries: queries
            }
          }),
          contentType: 'application/json',
          processData: false,
          success: function (data) {
            callbackFunc(false, data);
          },
          error: function () {
            callbackFunc(true);
          }
        });
      }
    },

    downloadLocalQueries: function () {
      arangoHelper.downloadLocalBlob(JSON.stringify(this.toJSON()), 'json');
    },

    saveImportQueries: function (file, callback) {
      var self = this;

      if (this.activeUser === 0) {
        return false;
      }

      var reader = new FileReader();
      if (frontendConfig.ldapEnabled) {
        if (file) {
          reader.readAsText(file, 'UTF-8');
          reader.onload = function (evt) {
            try {
              var obj = JSON.parse(evt.target.result);
              _.each(obj, function (val, key) {
                if (val.name && val.value && val.parameter) {
                  self.add({
                    name: val.name,
                    value: val.value,
                    parameter: val.parameter,
                  });
                }
              });
              self.saveCollectionQueries();
              if (callback) {
                callback();
              }
            } catch (e) {
              arangoHelper.arangoError('Query error', 'Queries could not be imported.');
              window.modalView.hide();
            }
          };
          reader.onerror = function (evt) {
            window.modalView.hide();
            arangoHelper.arangoError('Query error', 'Queries could not be imported.');
          };
        }
      } else {
        reader.readAsText(file);
        reader.onload = async () => {
          try {
            var data = reader.result;
            if (typeof data !== "string") {
              return;
            }
            var queries = JSON.parse(data);
            var sanitizedQueries = queries.map((query) => {
              return {
                name: query.name,
                value: query.value,
                parameter: query.parameter
              };
            });
            window.progressView.show('Fetching documents...');
            $.ajax({
              cache: false,
              type: 'POST',
              url: arangoHelper.databaseUrl('/_admin/aardvark/query/upload/' + encodeURIComponent(window.App.currentUser)),
              data: JSON.stringify(sanitizedQueries),
              contentType: 'application/json',
              processData: false,
              success: function () {
                window.progressView.hide();
                arangoHelper.arangoNotification('Queries successfully imported.');
                callback();
              },
              error: function (error) {
                window.progressView.hide();
                arangoHelper.arangoError('Query error', 'Queries could not be imported');
              }
            });
          } catch (e) {
            arangoHelper.arangoError('Query error', 'Queries could not be imported');
            window.progressView.hide();
          }
        };
      }
    }

  });
}());
