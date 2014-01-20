/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";
  window.ClusterCollections = Backbone.Collection.extend({
    model: window.ClusterCollection,
    
    url: "/_admin/aardvark/cluster/Collections",

    getList: function() {
      return [
        {
          name: "Documents",
          status: "ok"
        },
        {
          name: "Edges",
          status: "warning"
        },
        {
          name: "People",
          status: "critical"
        }
      ];
    }

  });
}());



