/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */
window.arangoCollections = Backbone.Collection.extend({
      url: '/_api/collection',
      parse: function(response)  {
          $.each(response.collections, function(key, val) {
            if (val.status == 2) {
              val.status = 'unloaded';
            }
            else if (val.status == 3) {
              val.status = 'loaded';
            }
          });
          return response.collections;
      },
      model: arangoCollection,
      getProperties: function (id) {
        $.ajax({
          type: "GET",
          url: "/_api/collection/" + id + "/properties",
          contentType: "application/json",
          processData: false,
          success: function(data) {
            return data;
          },
          error: function(data) {
            console.log("get properties failed");
          }
        });
        return false;
      },
      rename: function (id) {

      }
});
