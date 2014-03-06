/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, templateEngine, alert */

(function() {
  "use strict";

  window.ShowShardsView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("showShards.ejs"),

    events: {
      "change #selectDB" : "updateCollections",
      "change #selectCol" : "updateShards"
    },

    initialize: function() {
      this.dbservers = new window.ClusterServers([], {
        interval: 10000
      });
      this.dbservers.fetch({
        async : false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      this.dbs = new window.ClusterDatabases([], {
        interval: this.interval
      });
      this.cols = new window.ClusterCollections();
      this.shards = new window.ClusterShards()
    },

    updateCollections: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      var list = this.cols.getList(dbName);
      $("#selectCol").html("");
      _.each(_.pluck(this.cols.getList(dbName), "name"), function(c) {
        $("#selectCol").append("<option id=\"" + c + "\">" + c + "</option>");
      });
      this.updateShards();
    },

    updateShards: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      var colName = $("#selectCol").find(":selected").attr("id");
      var list = this.shards.getList(dbName, colName);
      $(".shardContainer").empty();
      _.each(list, function(s) {
        var item = $("#" + s.server + "Shards");
        $(".collectionName", item).html(s.server + ": " + s.shards.length)
        /* Will be needed in future
        _.each(s.shards, function(shard) {
          var shardIcon = document.createElement("span");
          shardIcon = $(shardIcon);
          shardIcon.toggleClass("fa");
          shardIcon.toggleClass("fa-th");
          item.append(shardIcon); 
        });
        */
      });
    },

    render: function() {
      $(this.el).html(this.template.render({
        names: this.dbservers.pluck("name"),
        dbs: _.pluck(this.dbs.getList(), "name")
      }));
      this.updateCollections();
    }
  });

}());
