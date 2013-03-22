/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */
window.arangoCollections = Backbone.Collection.extend({
      url: '/_api/collection',

      model: arangoCollection,

      searchOptions : {
        searchPhrase: null,
        includeSystem: false,
        includeDocument: true,
        includeEdge: true,
        includeLoaded: true,
        includeUnloaded: true
      },

      comparator : function(model) {
        return model.get('name').toLowerCase();
      },

      translateStatus : function (status) {
        if (status == 1) {
          return 'new born collection';
        }
        else if (status == 2) {
          return 'unloaded';
        }
        else if (status == 3) {
          return 'loaded';
        }
        else if (status == 4) {
          return 'in the process of being unloaded';
        }
        else if (status == 5) {
          return 'deleted';
        }
      },
      translateTypePicture : function (type) {
        if (type === 'document') {
          return "img/icon_document.png";
        }
        else if (type === 'edge') {
          return "img/icon_node.png";
        }
        else if (type === 'unknown') {
          return "img/icon_unknown.png";
        }
        else {
          return "img/icon_arango.png";
        }
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
          
        var searchPhrase = '';
        if (options.searchPhrase !== null) {
          searchPhrase = options.searchPhrase.toLowerCase();
        }

        this.models.forEach(function (model) {
          if (searchPhrase !== '' && model.get('name').toLowerCase().indexOf(searchPhrase) === -1) {
            // search phrase entered but current collection does not match?
            return;
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

        return result;
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
      checkCollectionName: function (name) {
      },
      newCollection: function (collName, wfs, isSystem, journalSize, collType) {
        var returnobj = {};
        returnobj.status = false;
        $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/collection",
          data: '{"name":' + JSON.stringify(collName) + ',"waitForSync":' + JSON.stringify(wfs) + ',"isSystem":' + JSON.stringify(isSystem) + journalSizeString + ',"type":' + collType + '}',
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            returnobj.status = true;
          },
          error: function(data) {
            returnobj.status = false;
            returnobj.errorMessage = JSON.parse(data.responseText).errorMessage;
          }
        });
        return returnobj;
      },
      renameCollection: function (id, name) {
        $.ajax({
          cache: false,
          type: "PUT",
          async: false, // sequential calls!
          url: "/_api/collection/" + id + "/rename",
          data: '{"name":"' + name + '"}',
          contentType: "application/json",
          processData: false,
          success: function(data) {
            arangoHelper.arangoNotification("Collection renamed");
            this.alreadyRenamed = true;
            window.arangoCollectionsStore.fetch({
              success: function () {
                window.collectionsView.render();
              }
            });
          },
          error: function(data) {
            arangoHelper.arangoNotification("Collection error");
            failed = true;
          }
        });
      },
      changeCollection: function (id, wfs, journalSize) {
        var self = this;
        $.ajax({
          cache: false,
          type: "PUT",
          async: false, // sequential calls!
          url: "/_api/collection/" + id + "/properties",
          data: '{"waitForSync":' + wfs + ',"journalSize":' + JSON.stringify(journalSize) + '}',
          contentType: "application/json",
          processData: false,
          success: function(data) {
            arangoHelper.arangoNotification("Saved collection properties");

            if (self.alreadyRenamed === true) {
              self.alreadyRenamed = false;
              window.arangoCollectionsStore.fetch({
                success: function () {
                  window.collectionsView.render();
                }
              });
            }

          },
          error: function(data) {
            arangoHelper.arangoNotification("Collection error");
            failed = true;
          }
        });
      },
      deleteCollection: function (id) {
        var returnval = false;
        $.ajax({
          cache: false,
          type: 'DELETE',
          url: "/_api/collection/" + id,
          async: false,
          success: function () {
            returnval = true;
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
