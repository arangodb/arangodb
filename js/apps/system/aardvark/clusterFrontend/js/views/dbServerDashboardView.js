/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.ServerDashboardView = window.DashboardView.extend({
    modal : true,

    hide: function() {
      window.App.showClusterView.startUpdating();
      this.stopUpdating();
    },

    render: function() {
      var self = this;
      window.modalView.hideFooter = true;
      window.modalView.show(
            "dashboardView.ejs",
            null,
            undefined,
            undefined,
            undefined,
            this.events

      );
      $('#modal-dialog').toggleClass("modal-chart-detail", true);
      window.DashboardView.prototype.render.bind(this)(true);
      window.modalView.hideFooter = false;


      $('#modal-dialog').on('hidden', function () {
            self.hide();
      });

      // Inject the closing x
      var closingX = document.createElement("button");
      closingX.className = "close";
      closingX.appendChild(
        document.createTextNode("×")
      );
      closingX = $(closingX);
      closingX.attr("data-dismiss", "modal");
      closingX.attr("aria-hidden", "true");
      closingX.attr("type", "button");
      $(".modal-body .headerBar:first-child")
        .toggleClass("headerBar", false)
        .toggleClass("modal-dashboard-header", true)
        .append(closingX);

    }
  });

}());
