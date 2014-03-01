/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.ServerDashboardView = window.dashboardView.extend({
    el: "#dbServerModal",

    events: {
      
    },

    render: function() {
      window.dashboardView.prototype.render.bind(this)();
      $(this.el).modal("show");
    }
  });

}());
