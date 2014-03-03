/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.ServerDashboardView = window.dashboardView.extend({
    el: "#dbServerModal",
    modal : true,
    events: {
      "hidden": "hide"
    },

    hide: function() {
      window.App.showClusterView.startUpdating();
      this.stopUpdating();
    },

    render: function() {
      window.dashboardView.prototype.render.bind(this)();
      $(this.el).modal("show");
    }
  });

}());
