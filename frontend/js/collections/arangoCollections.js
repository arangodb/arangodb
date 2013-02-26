/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */
window.arangoCollections = Backbone.Collection.extend({
      url: '/_api/collection',
      model: arangoCollection,
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

      getProperties: function (id) {
        var data2;
        $.ajax({
          type: "GET",
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
        var returnval = false;
        $.ajax({
          type: "POST",
          url: "/_api/collection",
          data: '{"name":' + JSON.stringify(collName) + ',"waitForSync":' + JSON.stringify(wfs) + ',"isSystem":' + JSON.stringify(isSystem) + journalSizeString + ',"type":' + collType + '}',
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            returnval = true;
          },
          error: function(data) {
            returnval = false;
          }
        });
        return returnval;
      },
      renameCollection: function (id, name) {
        $.ajax({
          type: "PUT",
          async: false, // sequential calls!
          url: "/_api/collection/" + id + "/rename",
          data: '{"name":"' + name + '"}',
          contentType: "application/json",
          processData: false,
          success: function(data) {
            alert("Collection renamed");
          },
          error: function(data) {
            alert(getErrorMessage(data));
            failed = true;
          }
        });
      },
      changeCollection: function (id, wfs, journalSize) {
        $.ajax({
          type: "PUT",
          async: false, // sequential calls!
          url: "/_api/collection/" + id + "/properties",
          data: '{"waitForSync":' + wfs + ',"journalSize":' + JSON.stringify(journalSize) + '}',
          contentType: "application/json",
          processData: false,
          success: function(data) {
            alert("Saved collection properties");
          },
          error: function(data) {
            alert(getErrorMessage(data));
            failed = true;
          }
        });
      },
      deleteCollection: function (id) {
        var returnval = false;
        $.ajax({
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
          type: 'PUT',
          url: "/_api/collection/" + id + "/load",
          success: function () {
          },
          error: function (data) {
            var temp = JSON.parse(data.responseText);
            alert("Error: " + JSON.stringify(temp.errorMessage));
          }
        });
      },
      unloadCollection: function (id) {
        $.ajax({
          type: 'PUT',
          url: "/_api/collection/" + id + "/unload",
          success: function () {
          },
          error: function () {
            var temp = JSON.parse(data.responseText);
            alert("Error: " + JSON.stringify(temp.errorMessage));
          }
        });
      }
});
