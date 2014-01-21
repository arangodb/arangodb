/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterShards = Backbone.Collection.extend({

    model: window.ClusterShard,
    
    url: function() {
      return "/_admin/aardvark/cluster/"
        + this.dbname + "/"
        + this.colname + "/"
        + "Shards/"
        + this.server;
    },

    getList: function(dbname, colname, server) {
      this.dbname = dbname;
      this.colname = colname;
      this.server = server;
      this.fetch({
        async: false
      });
      return this.map(function(m) {
        return m.forList();
      });
    }

  });
}());



