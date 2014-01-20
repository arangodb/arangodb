/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";
  window.ClusterCollections = Backbone.Collection.extend({
    model: window.ClusterCollection,
    
    url: "/_admin/aardvark/cluster/Collections",

    getList: function() {
      this.fetch({
        async: false
      });
      return this.map(function(m) {
        return m.forList();
      });
    }

  });
}());



