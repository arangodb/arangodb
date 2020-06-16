/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, window, arangoCollectionModel, $, arangoHelper, _ */
(function () {
  'use strict';

  window.ArangoCollections = Backbone.Collection.extend({
    url: arangoHelper.databaseUrl('/_api/collection'),

    model: arangoCollectionModel,

    searchOptions: {
      searchPhrase: null,
      includeSystem: false,
      includeDocument: true,
      includeEdge: true,
      includeLoaded: true,
      includeUnloaded: true,
      sortBy: 'name',
      sortOrder: 1
    },

    translateStatus: function (status) {
      switch (status) {
        case 0:
          return 'corrupted';
        case 1:
          return 'new born collection';
        case 2:
          return 'unloaded';
        case 3:
          return 'loaded';
        case 4:
          return 'unloading';
        case 5:
          return 'deleted';
        case 6:
          return 'loading';
        default:
          return 'unknown';
      }
    },

    translateTypePicture: function (type) {
      var returnString = '';
      switch (type) {
        case 'document':
          returnString += 'fa-file-text-o';
          break;
        case 'edge':
          returnString += 'fa-share-alt';
          break;
        case 'unknown':
          returnString += 'fa-question';
          break;
        default:
          returnString += 'fa-cogs';
      }
      return returnString;
    },

    parse: function (response) {
      var self = this;
      _.each(response.result, function (val) {
        val.isSystem = arangoHelper.isSystemCollection(val);
        val.type = arangoHelper.collectionType(val);
        val.status = self.translateStatus(val.status);
        val.picture = self.translateTypePicture(val.type);
      });
      return response.result;
    },

    getPosition: function (name) {
      var list = this.getFiltered(this.searchOptions); var i;
      var prev = null;
      var next = null;

      for (i = 0; i < list.length; ++i) {
        if (list[i].get('name') === name) {
          if (i > 0) {
            prev = list[i - 1];
          }
          if (i < list.length - 1) {
            next = list[i + 1];
          }
        }
      }

      return { prev: prev, next: next };
    },

    getFiltered: function (options) {
      var result = [];
      var searchPhrases = [];

      if (options.searchPhrase !== null) {
        var searchPhrase = options.searchPhrase.toLowerCase();
        // kick out whitespace
        searchPhrase = searchPhrase.replace(/\s+/g, ' ').replace(/(^\s+|\s+$)/g, '');
        searchPhrases = searchPhrase.split(' ');
      }

      this.models.forEach(function (model) {
        // search for name(s) entered
        if (searchPhrases.length > 0) {
          var lowerName = model.get('name').toLowerCase(); var i;
          // all phrases must match!
          for (i = 0; i < searchPhrases.length; ++i) {
            if (lowerName.indexOf(searchPhrases[i]) === -1) {
              // search phrase entered but current collection does not match?
              return;
            }
          }
        }

        if (options.includeSystem === false && model.get('isSystem')) {
          // system collection?
          return;
        }
        if (options.includeEdge === false && model.get('type') === 'edge') {
          return;
        }
        if (options.includeDocument === false && model.get('type') === 'document') {
          return;
        }
        if (options.includeLoaded === false && model.get('status') === 'loaded') {
          return;
        }
        if (options.includeUnloaded === false && model.get('status') === 'unloaded') {
          return;
        }

        result.push(model);
      });

      result.sort(function (l, r) {
        var lValue, rValue;
        if (options.sortBy === 'type') {
          // we'll use first type, then name as the sort criteria
          // this is because when sorting by type, we need a 2nd criterion (type is not unique)
          lValue = l.get('type') + ' ' + l.get('name').toLowerCase();
          rValue = r.get('type') + ' ' + r.get('name').toLowerCase();
        } else {
          lValue = l.get('name').toLowerCase();
          rValue = r.get('name').toLowerCase();
        }
        if (lValue !== rValue) {
          return options.sortOrder * (lValue < rValue ? -1 : 1);
        }
        return 0;
      });

      return result;
    },

    newCollection: function (object, callback) {
      var data = {};
      data.name = object.collName;
      data.waitForSync = object.wfs;
      data.isSystem = object.isSystem;
      data.type = parseInt(object.collType, 10);
      if (object.shards) {
        data.numberOfShards = object.shards;
      }
      data.shardKeys = object.shardKeys;

      if (object.smartJoinAttribute &&
          object.smartJoinAttribute !== '') {
        data.smartJoinAttribute = object.smartJoinAttribute;
      }
      data.distributeShardsLike = object.distributeShardsLike;

      var pattern = new RegExp(/^[0-9]+$/);
      if (object.replicationFactor) {
        data.replicationFactor = object.replicationFactor;
        if (pattern.test(object.replicationFactor)) {
          // looks like a number
          data.replicationFactor = JSON.parse(object.replicationFactor);
        }
      }

      if (object.writeConcern) {
        data.writeConcern = object.writeConcern;
        if (pattern.test(object.writeConcern)) {
          // looks like a number
          data.writeConcern = JSON.parse(object.writeConcern);
        }
      }

      $.ajax({
        cache: false,
        type: 'POST',
        url: arangoHelper.databaseUrl('/_api/collection'),
        data: JSON.stringify(data),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    }
  });
}());
