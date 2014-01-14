/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterShardsView = Backbone.View.extend({
    
    el: '#clusterShards',

    template: templateEngine.createTemplate("clusterShardsView.ejs"),

    render: function(){
      $(this.el).html(this.template.render({}));
      return this;
    }

  });

}());

