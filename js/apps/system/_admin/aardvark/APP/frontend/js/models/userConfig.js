/* global frontendConfig */
window.UserConfig = Backbone.Model.extend({
  defaults: {
    graphs: '',
    queries: []
  },

  ldapEnabled: false,

  initialize: function (options) {
    this.ldapEnabled = options.ldapEnabled;
    this.getUser();
  },

  model: window.UserConfigModel,

  fetch: function (options) {
    options = _.extend({parse: true}, options);
    var model = this;
    var success = options.success;

    if (this.ldapEnabled) {
      this.getLocalConfig();
      options.success = (function (resp) {
        // if success function available, call it
        if (success) {
          success.call(options.context, model, resp, options);
        }
      })();
    } else {
      // if ldap is not enabled -> call Backbone's fetch method
      return Backbone.Collection.prototype.fetch.call(this, options);
    }
  },

  getConfigPath: function () {
    return frontendConfig.db + '-' + this.username + '-config';
  },

  getLocalConfig: function () {
    var item = sessionStorage.getItem(this.getConfigPath());
    try {
      item = JSON.parse(item);
    } catch (ignore) {
    }

    if (item) {
      this.set(item);
    } else {
      this.set({});
    }
    return this.toJSON();
  },

  saveLocalConfig: function () {
    sessionStorage.setItem(this.getConfigPath(), JSON.stringify(this.toJSON()));
  },

  getLocalItem: function (keyName, callback) {
    if (callback) {
      callback(this.get(keyName));
    }
    return this.get(keyName);
  },

  setLocalItem: function (keyName, keyValue, callback) {
    try {
      this.set(keyName, keyValue);
      this.saveLocalConfig();
      if (callback) {
        callback();
      }
    } catch (ignore) {
    }
  },

  getUser: function () {
    if (window.App.currentUser) {
      this.username = window.App.currentUser;
    } else {
      this.username = 'root';
    }
  },

  parse: function (response) {
    return response.result;
  },

  url: function () {
    return arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.username) + '/config');
  },

  setItem: function (keyName, keyValue, callback) {
    // keyName: Name where the objects will be stored.
    // keyValue: Object the user wants to store.
    // callback: Function which will be executed after successful exit.

    this.fetch({
      success: function (data) {
        storeToDB(data.toJSON());
      },
      error: function (data) {
        if (data.responseJSON && data.responseJSON.errorMessage && data.responseJSON.errorNum) {
          arangoHelper.arangoError(
            'Graph Config',
            `${data.responseJSON.errorMessage}[${data.responseJSON.errorNum}]`
          );
        } else {
          arangoHelper.arangoError(
            'Graph Config',
            'Could not fetch graph configuration. Cannot save changes!'
          );
        }
      }
    });

    const mergeDatabaseDataWithLocalChanges = (dataFromDB, keyName, newObject) => {
      // Currently a local merge is required as the "_api/user/<user>/config"
      // API does not do a merge internally. It does a replacement instead.

      const isObject = (varToCheck) => {
        if (typeof varToCheck === 'object' && !Array.isArray(varToCheck) && varToCheck !== null) {
          return true;
        } else {
          return false;
        }
      };

      if (!dataFromDB) {
        return newObject;
      }

      // Format notice
      // dataFromDB<object>: dataFromDB.result.keyName
      if (dataFromDB.result && dataFromDB.result[keyName]) {
        const existingEntry = dataFromDB.result[keyName];
        // We've found available data we need to merge
        if (isObject(existingEntry) && isObject(newObject)) {
          return {...existingEntry, ...newObject};
        } else {
          return newObject;
        }
      }

      return newObject;
    };

    // Method to save the actual item
    const storeToDB = (dataFromDB) => {
      const dataObjectToStore = mergeDatabaseDataWithLocalChanges(dataFromDB, keyName, keyValue);

      if (this.ldapEnabled) {
        this.setLocalItem(keyName, dataObjectToStore, callback);
      } else {
        // url PUT /_api/user/<username>/config/<key>
        var self = this;

        $.ajax({
          type: 'PUT',
          cache: false,
          url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.username) + '/config/' + encodeURIComponent(keyName)),
          contentType: 'application/json',
          processData: false,
          data: JSON.stringify({value: dataObjectToStore}),
          async: true,
          success: function () {
            self.set(keyName, dataObjectToStore);

            if (callback) {
              callback(dataObjectToStore);
            }
          },
          error: function () {
            arangoHelper.arangoError('User configuration', 'Could not update user configuration for key: ' + keyName);
          }
        });
      }
    };
  },

  getItem: function (keyName, callback) {
    if (this.ldapEnabled) {
      this.getLocalItem(keyName, callback);
    } else {
      // url GET /_api/user/<username>/config/<key>
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.username) + '/config/' + encodeURIComponent(keyName)),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (keyValue) {
          callback(keyValue);
        },
        error: function () {
          arangoHelper.arangoError('User configuration', 'Could not fetch user configuration for key: ' + keyName);
        }
      });
    }
  }

});
