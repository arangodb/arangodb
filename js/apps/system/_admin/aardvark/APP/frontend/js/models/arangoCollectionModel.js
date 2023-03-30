/* global window, Backbone, $, arangoHelper, frontendConfig */
(function () {
  'use strict';
  window.arangoCollectionModel = Backbone.Model.extend({
    idAttribute: 'name',

    urlRoot: arangoHelper.databaseUrl('/_api/collection'),
    defaults: {
      id: '',
      name: '',
      status: '',
      type: '',
      isSystem: false,
      picture: '',
      locked: false,
      desc: undefined
    },

    getProperties: function (callback) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/properties'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    },
    getShards: function (callback) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/shards?details=true'),
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
    getFigures: function (callback) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/figures'),
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
    getFiguresCombined: function (callback, isCluster) {
      var self = this;
      var propertiesCallback = function (error, data) {
        if (error) {
          callback(true);
        } else {
          var figures = data;
          self.getProperties(function (error, data) {
            if (error) {
              callback(true, figures);
            } else {
              callback(false, Object.assign(figures, { properties: data }));
            }
          });
        }
      };
      var shardsCallback = function (error, data) {
        if (error) {
          propertiesCallback(true);
        } else {
          var figures = data;
          if (isCluster) {
            self.getShards(function (error, data) {
              if (error) {
                propertiesCallback(true, data);
              } else {
                propertiesCallback(false, Object.assign(figures, { shards: data.shards }));
              }
            });
          } else {
            propertiesCallback(false, figures);
          }
        }
      };
      this.getFigures(shardsCallback);
    },

    getShardCounts: function (callback) {
      if (!frontendConfig.isCluster) {
        return;
      }
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/count?details=true'),
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

    getRevision: function (callback, figures) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/revision'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data, figures);
        },
        error: function () {
          callback(true);
        }
      });
    },

    truncateCollection: function () {
      var self = this;
      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/truncate'),
        success: function () {
          arangoHelper.arangoNotification('Collection truncated.');
        },
        error: function (err) {
          arangoHelper.arangoError('Collection error: ' + err.responseJSON.errorMessage);
        }
      });
    },

    warmupCollection: function () {
      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/loadIndexesIntoMemory'),
        success: function () {
          arangoHelper.arangoNotification('Loading indexes into memory.');
        },
        error: function (err) {
          arangoHelper.arangoError('Collection error: ' + err.responseJSON.errorMessage);
        }
      });
    },

    renameCollection: function (name, callback) {
      var self = this;

      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/rename'),
        data: JSON.stringify({ name: name }),
        contentType: 'application/json',
        processData: false,
        success: function () {
          self.set('name', name);
          callback(false);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    },

    changeCollection: function (wfs, replicationFactor, writeConcern, cacheEnabled, callback) {
      if (wfs === 'true') {
        wfs = true;
      } else if (wfs === 'false') {
        wfs = false;
      }
      var data = {
        waitForSync: wfs
      };

      if (cacheEnabled === 'true' || cacheEnabled === true) {
        cacheEnabled = true;
      } else {
        cacheEnabled = false;
      }
      if (cacheEnabled !== null && cacheEnabled !== undefined) {
        data.cacheEnabled = cacheEnabled;
      }

      if (replicationFactor) {
        data.replicationFactor = parseInt(replicationFactor, 10);
      }
      if (writeConcern) {
        // not an error. writeConcern is stored in minReplicationFactor for historical reasons
        data.minReplicationFactor = parseInt(writeConcern, 10);
      }

      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/properties'),
        data: JSON.stringify(data),
        contentType: 'application/json',
        processData: false,
        success: function () {
          callback(false);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    },

    changeComputedValues: function (computedValues, callback) {
      if (!computedValues) {
        computedValues = null;
      }

      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/properties'),
        data: JSON.stringify({ computedValues: computedValues }),
        contentType: 'application/json',
        processData: false,
        success: function () {
          callback(false);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    },

    changeValidation: function (validation, callback) {
      if (!validation) {
        validation = null;
      }

      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('name')) + '/properties'),
        data: JSON.stringify({ schema: validation }),
        contentType: 'application/json',
        processData: false,
        success: function () {
          callback(false);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    }

  });
}());
