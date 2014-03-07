/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";
  window.ShutdownButtonView = Backbone.View.extend({
    el: '#navigationBar',

    events: {
      "click #clusterShutdown"  : "clusterShutdown"
    },

    initialize: function() {
      this.overview = this.options.overview;
    },

    template: templateEngine.createTemplate("shutdownButtonView.ejs"),

    clusterShutdown : function() {
      this.overview.stopUpdating();
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is shutting down');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/shutdown",
        success: function(data) {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.navigate("handleClusterDown", {trigger: true});
        }
      });
    },

    render: function () {
      $(this.el).html(this.template.render({}));
      return this;
    },

    unrender: function() {
      $(this.el).html("");
    }
  });
}());

