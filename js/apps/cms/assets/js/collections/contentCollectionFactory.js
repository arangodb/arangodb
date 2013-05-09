app = app || {};

(function () {
  "use strict";
  
  app.ContentCollectionFactory = function () {
    this.create = function(name, callback) {
      $.get("structs/" + name, function(data) {
        var defaults = {},
          columns = [];
        _.each(data.attributes, function (content, name) {
          switch (content.type) {
            case "number":
              defaults[name] = 0;
              break;
            case "string":
              defaults[name] = "";
              break;
            default:
              defaults[name] = "";
              break;
          }
          columns.push({
            name: name,
            type: content.type
          });
        });
        columns = _.sortBy(columns, function(c){ return c.name; });
        var Content = Backbone.Model.extend({
          defaults: defaults,
          idAttribute: "_key"
        }),
          ContentCollection = Backbone.Collection.extend({
            model: Content,
            url: "content/" + name,
            
            getColumns: function() {
              return columns;
            }
          }),
          collection = new ContentCollection();
        callback(collection);
      });
    };
  };
}());