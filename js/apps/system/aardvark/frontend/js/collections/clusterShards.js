/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";
  window.ClusterShards = Backbone.Collection.extend({
    model: window.ClusterShard,
    
    url: "/_admin/aardvark/cluster/Shards",

    getList: function() {
      return [
        {
          name: "Shard 1",
          status: "ok"
        },
        {
          name: "Shard 2",
          status: "warning"
        },
        {
          name: "Shard 3",
          status: "critical"
        }
      ];
    }

  });
}());



