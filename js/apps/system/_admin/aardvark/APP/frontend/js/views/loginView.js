/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, arangoHelper, window, setTimeout, $, templateEngine*/

(function() {
  "use strict";
  window.loginView = Backbone.View.extend({
    el: '#content',
    el2: '.header',
    el3: '.footer',

    init: function () {
    },

    events: {
      "submit #loginForm" : "login"
    },

    template: templateEngine.createTemplate("loginView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({}));
      $(this.el2).hide();
      $(this.el3).hide();

      $('#loginUsername').focus();

      return this;
    },

    login: function (e) {
      e.preventDefault();
      var username = $('#loginUsername').val();
      var password = $('#loginPassword').val();

      if (!username) {
        //Heiko: Form-Validator - please fill out all req. fields
        //console.log(username, 'empty');
        return;
      }
      username = this.collection.login(username, password);

      if (username) {
        $(this.el2).show();
        $(this.el3).show();
        window.location.reload();
        $('#currentUser').text(username);
        this.collection.loadUserSettings();
      }
    }

  });
}());
