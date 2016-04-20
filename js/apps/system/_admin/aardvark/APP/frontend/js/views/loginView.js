/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, arangoHelper, window, setTimeout, $, templateEngine*/

(function() {
  "use strict";
  window.loginView = Backbone.View.extend({
    el: '#content',
    el2: '.header',
    el3: '.footer',

    events: {
      "submit #loginForm" : "login",
      "keypress #loginForm input" : "clear",
      "change #loginForm input" : "clear"
    },

    template: templateEngine.createTemplate("loginView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({}));
      $('#loginDatabase').val('_system');
      $(this.el2).hide();
      $(this.el3).hide();

      $('#loginUsername').focus();

      return this;
    },

    clear: function () {
      $('#loginForm input').removeClass("form-error");
      $('.wrong-credentials').hide();
    },

    login: function (e) {
      e.preventDefault();
      var username = $('#loginUsername').val();
      var password = $('#loginPassword').val();
      var database = $('#loginDatabase').val();

      if (!username) {
        //Heiko: Form-Validator - please fill out all req. fields
        return;
      }

      var callback = function(error, username) {
        var callback2 = function(error) {
          if (error) {
            arangoHelper.arangoError("User", "Could not fetch user settings"); 
          }
        };

        if (error) {
          $('#loginForm input').addClass("form-error");
          $('.wrong-credentials').show();
        }
        else {
          var currentDB = window.location.pathname.split('/')[2];
          if (currentDB !== database) {
            var path = window.location.origin + window.location.pathname.replace(currentDB, database);
            window.location.href = path;
            $(this.el2).show();
            $(this.el3).show();
            $('#currentUser').text(username);
            this.collection.loadUserSettings(callback2);
            return;
          }
          
          $(this.el2).show();
          $(this.el3).show();
          window.location.reload();
          $('#currentUser').text(username);
          this.collection.loadUserSettings(callback2);
        } 
      }.bind(this);

      this.collection.login(username, password, database, callback);

    }

  });
}());
