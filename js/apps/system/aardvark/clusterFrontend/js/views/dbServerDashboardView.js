/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.ServerDashboardView = window.newDashboardView.extend({
    modal : true,

    hide: function() {
      window.App.showClusterView.startUpdating();
      this.stopUpdating();
    },

    render: function() {
      var self = this;
      window.modalView.hideFooter = true;
      window.modalView.show(
            "newDashboardView.ejs",
            null,
            undefined,
            undefined,
            undefined,
            this.events

      );
      window.newDashboardView.prototype.render.bind(this)(true);
      window.modalView.hideFooter = false;


      $('#modal-dialog').on('hidden', function () {
            self.hide();
      });
      $('.modal-body').css({"max-height": "90%" });
      $('#modal-dialog').toggleClass("modalChartDetail", true);
    }
  });

}());
