/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, document, EJS, _, arangoHelper, window, setTimeout, $, templateEngine*/

(function() {
  "use strict";
  window.loginView = Backbone.View.extend({
    el: '#content',
    el2: '.header',
    el3: '.footer',
    loggedIn: false,

    events: {
      "submit #loginForm" : "goTo",
      "keyup #loginForm input" : "validate",
      "change #loginForm input" : "validate",
      "focusout #loginForm input" : "validate"
    },

    template: templateEngine.createTemplate("loginView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({}));
      $(this.el2).hide();
      $(this.el3).hide();

      $('#loginUsername').focus();

      return this;
    },

    clear: function () {
      $('#loginForm input').removeClass("form-error");
      $('.wrong-credentials').hide();
    },

    validate: function(event) {
      this.clear();
      var self = this;

      var username = $('#loginUsername').val();
      var password = $('#loginPassword').val();

      if (!username) {
        //do not send unneccessary requests if no user is given
        return;
      }

      var callback = function(error) {
        if (error) {
          if (event.type === 'focusout') {
            //$('#loginForm input').addClass("form-error");
            $('.wrong-credentials').show();
            $('#loginDatabase').html('');
            $('#loginDatabase').append(
              '<option>_system</option>'
            ); 
            $('#loginDatabase').prop('disabled', true);
            $('#submitLogin').prop('disabled', true);
          }
        }
        else {
          $('.wrong-credentials').hide();

          self.loggedIn = true;
          //get list of allowed dbs
          $.ajax("/_api/database/user").success(function(data) {
            //enable db select and login button
            $('#loginDatabase').prop('disabled', false);
            $('#submitLogin').prop('disabled', false);
            $('#loginDatabase').html('');
            //fill select with allowed dbs
            _.each(data.result, function(db) {
              $('#loginDatabase').append(
                '<option>' + db + '</option>'
              ); 
            });
          });
        }
      }.bind(this);

      this.collection.login(username, password, callback);
    },

    goTo: function (e) {
      e.preventDefault();
      var username = $('#loginUsername').val();
      var database = $('#loginDatabase').val();

      var callback2 = function(error) {
        if (error) {
          arangoHelper.arangoError("User", "Could not fetch user settings"); 
        }
      };

      var currentDB = window.location.pathname.split('/')[2];
      var path = window.location.origin + window.location.pathname.replace(currentDB, database);
      window.location.href = path;
      $(this.el2).show();
      $(this.el3).show();
      $('#currentUser').text(username);
      this.collection.loadUserSettings(callback2);
    }

  });
}());
