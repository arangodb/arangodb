/*jshint browser: true, unused: false */
/*global require, exports, Backbone, window, arangoCollectionModel, $, arangoHelper, data, _ */
(function() {
  "use strict";

  window.arangoCollections = Backbone.Collection.extend({
      url: '/_api/collection',

      model: arangoCollectionModel,

      searchOptions : {
        searchPhrase: null,
        includeSystem: false,
        includeDocument: true,
        includeEdge: true,
        includeLoaded: true,
        includeUnloaded: true,
        sortBy: 'name',
        sortOrder: 1
      },

      translateStatus : function (status) {
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
            return 'in the process of being unloaded';
          case 5:
            return 'deleted';
          case 6:
            return 'loading';
          default:
            return;
        }
      },

      translateTypePicture : function (type) {
        var returnString = "img/icon_";
        switch (type) {
          case 'document':
            returnString += "document.png";
            break;
          case 'edge':
            returnString += "node.png";
            break;
          case 'unknown':
            returnString += "unknown.png";
            break;
          default:
            returnString += "arango.png";
        }
        return returnString;
      },

      parse: function(response)  {
        var self = this;
        _.each(response.collections, function(val) {
          val.isSystem = arangoHelper.isSystemCollection(val);
          val.type = arangoHelper.collectionType(val);
          val.status = self.translateStatus(val.status);
          val.picture = self.translateTypePicture(val.type);
        });
        return response.collections;
      },

      getPosition : function (name) {
        var list = this.getFiltered(this.searchOptions), i;
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

      getFiltered : function (options) {
        var result = [ ];
        var searchPhrases = [ ];

        if (options.searchPhrase !== null) {
          var searchPhrase = options.searchPhrase.toLowerCase();
          // kick out whitespace
          searchPhrase = searchPhrase.replace(/\s+/g, ' ').replace(/(^\s+|\s+$)/g, '');
          searchPhrases = searchPhrase.split(' ');
        }

        this.models.forEach(function (model) {
          // search for name(s) entered
          if (searchPhrases.length > 0) {
            var lowerName = model.get('name').toLowerCase(), i;
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
          }
          else {
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

      newCollection: function (collName, wfs, isSystem, journalSize, collType, shards, keys) {
        var returnobj = {};
        var data = {};
        data.name = collName;
        data.waitForSync = wfs;
        if (journalSize > 0) {
          data.journalSize = journalSize;
        }
        data.isSystem = isSystem;
        data.type = parseInt(collType, 10);
        if (shards) {
          data.numberOfShards = shards;
          data.shardKeys = keys;
        }
        returnobj.status = false;
        $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/collection",
          data: JSON.stringify(data),
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            returnobj.status = true;
            returnobj.data = data;
          },
          error: function(data) {
            returnobj.status = false;
            returnobj.errorMessage = JSON.parse(data.responseText).errorMessage;
          }
        });
        return returnobj;
      }
  });
}());
