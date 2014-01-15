/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterCoordinatorView = Backbone.View.extend({
    
    el: '#clusterServers',

    template: templateEngine.createTemplate("clusterCoordinatorView.ejs"),

    render: function(){
      $(this.el).html(this.template.render({}));
      return this;
    }

  });

}());
