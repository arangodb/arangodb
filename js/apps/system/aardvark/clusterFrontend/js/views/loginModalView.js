/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.LoginModalView = Backbone.View.extend({

    template: templateEngine.createTemplate("loginModal.ejs"),
    el: '#modalPlaceholder',

    events: {
      "click #confirmLogin": "confirmLogin",
      "hidden #loginModalLayer": "hidden"
    },

    hidden: function () {
      this.undelegateEvents();
      window.App.isCheckingUser = false;
      $(this.el).html("");
    },

    confirmLogin: function() {
      var uName = $("#username").val();
      var passwd = $("#password").val();
      window.App.clusterPlan.storeCredentials(uName, passwd);
      this.hideModal();
    },

    hideModal: function () {
      $('#loginModalLayer').modal('hide');
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      $('#loginModalLayer').modal('show');
    }
  });

}());
