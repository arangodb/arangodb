/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, document, arangoCollection,arangoHelper,dashboardView,arangoDatabase*/

(function() {
  "use strict";

  window.ClusterRouter = Backbone.Router.extend({

    routes: {
      ""                       : "showCluster"
    },

    initialize: function () {
      /*
      this.footerView = new window.FooterView();
      this.footerView.render();
      */
    },
    showCluster: function() {
      if (!this.showClusterView) {
        this.showClusterView = new window.ShowClusterView();
      }
      this.showClusterView.render();
    },

    handleResize: function() {
     // Not needed here
    }
  });

}());
