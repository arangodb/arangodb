/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, arangoCollection, $, arangoHelper, data */

window.arangoCollections = Backbone.Collection.extend({
      url: '/_api/collection',

      model: arangoCollection,

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
        var returnString;
        if (status === 0) {
          returnString = 'corrupted';
        }
        if (status === 1) {
          returnString = 'new born collection';
        }
        else if (status === 2) {
          returnString = 'unloaded';
        }
        else if (status === 3) {
          returnString = 'loaded';
        }
        else if (status === 4) {
          returnString = 'in the process of being unloaded';
        }
        else if (status === 5) {
          returnString = 'deleted';
        }
        return returnString;
      },
      translateTypePicture : function (type) {
        var returnString;
        if (type === 'document') {
          returnString = "img/icon_document.png";
        }
        else if (type === 'edge') {
          returnString = "img/icon_node.png";
        }
        else if (type === 'unknown') {
          returnString = "img/icon_unknown.png";
        }
        else {
          returnString = "img/icon_arango.png";
        }
        return returnString;
      },
      parse: function(response)  {
        var that = this;

        $.each(response.collections, function(key, val) {
          val.isSystem = arangoHelper.isSystemCollection(val.name);
          val.type = arangoHelper.collectionType(val);
          val.status = that.translateStatus(val.status);
          val.picture = that.translateTypePicture(val.type);
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
      getIndex: function (id) {
        var data2;
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/index/?collection=" + id,
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            data2 = data;
          },
          error: function(data) {
            data2 = data;
          }
        });
        return data2;
      },
      getProperties: function (id) {
        var data2;
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/collection/" + id + "/properties",
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            data2 = data;
          },
          error: function(data) {
            data2 = data;
          }
        });
        return data2;
      },
      getFigures: function (id) {
        var data2;
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/collection/" + id + "/figures",
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            data2 = data;
          },
          error: function(data) {
            data2 = data;
          }
        });
        return data2;
      },
      getRevision: function (id) {
        var data2;
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/collection/" + id + "/revision",
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            data2 = data;
          },
          error: function(data) {
            data2 = data;
          }
        });
        return data2;
      },
      checkCollectionName: function (name) {
      },
      newCollection: function (collName, wfs, isSystem, journalSize, collType) {
        var returnobj = {};
        returnobj.status = false;
        $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/collection",
          data:
            '{"name":' + JSON.stringify(collName) +
            ',"waitForSync":'+
            JSON.stringify(wfs)+
            ',"isSystem":'+
            JSON.stringify(isSystem)+
            ',"type":'+
            collType + '}',
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
      },
      renameCollection: function (id, name) {
        var result = false;
        $.ajax({
          cache: false,
          type: "PUT",
          async: false, // sequential calls!
          url: "/_api/collection/" + id + "/rename",
          data: '{"name":"' + name + '"}',
          contentType: "application/json",
          processData: false,
          success: function(data) {
            result = true;
            this.alreadyRenamed = true;
          },
          error: function(data) {
            try {
              var parsed = JSON.parse(data.responseText);
              result = parsed.errorMessage;
            }
            catch (e) {
              result = false;
            }
          }
        });
        return result;
      },
      changeCollection: function (id, wfs, journalSize) {
        var self = this;
        var result = false;

        $.ajax({
          cache: false,
          type: "PUT",
          async: false, // sequential calls!
          url: "/_api/collection/" + id + "/properties",
          data: '{"waitForSync":' + wfs + ',"journalSize":' + JSON.stringify(journalSize) + '}',
          contentType: "application/json",
          processData: false,
          success: function(data) {
            result = true;
            if (self.alreadyRenamed === true) {
              self.alreadyRenamed = false;
            }
          },
          error: function(data) {
            try {
              var parsed = JSON.parse(data.responseText);
              result = parsed.errorMessage;
            }
            catch (e) {
              result = false;
            }
          }
        });
        return result;
      },
      deleteCollection: function (id) {
        var returnval = false;
        var self = this;
        $.ajax({
          cache: false,
          type: 'DELETE',
          url: "/_api/collection/" + id,
          async: false,
          success: function () {
            returnval = true;
            self.remove(self.findWhere({name: id}));
            window.collectionsView.render();
          },
          error: function () {
            returnval = false;
          }
        });
        return returnval;
      },
      loadCollection: function (id) {
        $.ajax({
          cache: false,
          type: 'PUT',
          url: "/_api/collection/" + id + "/load",
          success: function () {
            arangoHelper.arangoNotification('Collection loaded');
            window.arangoCollectionsStore.fetch({
              success: function () {
                window.collectionsView.render();
              }
            });
          },
          error: function (data) {
            var temp = JSON.parse(data.responseText);
            arangoHelper.arangoError('Collection error');
          }
        });
      },
      unloadCollection: function (id) {
        $.ajax({
          cache: false,
          type: 'PUT',
          url: "/_api/collection/" + id + "/unload",
          success: function () {
            arangoHelper.arangoNotification('Collection unloaded');
            window.arangoCollectionsStore.fetch({
              success: function () {
                window.collectionsView.render();
              }
            });
          },
          error: function () {
            var temp = JSON.parse(data.responseText);
            arangoHelper.arangoError('Collection error');
          }
        });
      }
});
