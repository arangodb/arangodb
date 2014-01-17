/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterShardsView = Backbone.View.extend({
    
    el: '#clusterShards',

    template: templateEngine.createTemplate("clusterShardsView.ejs"),

    initialize: function() {
      this.fakeData = {
        shards: [
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
        ]
      };
    },

    render: function(){
      $(this.el).html(this.template.render({
        shards: this.fakeData.shards
      }));
      return this;
    }

  });

}());

