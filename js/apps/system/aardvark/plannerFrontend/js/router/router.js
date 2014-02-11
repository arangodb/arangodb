/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, document, arangoCollection,arangoHelper,dashboardView,arangoDatabase*/

(function() {
  "use strict";

  window.Router = Backbone.Router.extend({
    routes: {
      "planTest"                                : "planTest"
    },

    planTest: function() {
              console.log("Startup");
      if (!this.planTestView) {
        this.planTestView = new window.PlanTestView();
      }
              console.log("Renderung");
      this.planTestView.render();
    },

    initialize: function () {
                  console.log("initio");
      /*
      this.footerView = new window.FooterView();
      this.footerView.render();
      */
    }
  });

}());
