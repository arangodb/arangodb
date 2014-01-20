/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterShards = Backbone.Collection.extend({

    model: window.ClusterShard,
    
    url: "/_admin/aardvark/cluster/Shards",

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



