/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterDashboardView = Backbone.View.extend({
    
    el: '#content',

    template: templateEngine.createTemplate("clusterDashboardView.ejs"),

    render: function(){
      $(this.el).html(this.template.render({}));
      this.overView = new window.ClusterOverviewView();
      this.overView.render();
      return this;
    }

  });

}());
