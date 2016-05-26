/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, document, EJS, _, arangoHelper, window, setTimeout, $, templateEngine, frontendConfig*/

(function() {
  "use strict";
  window.loginView = Backbone.View.extend({
    el: '#content',
    el2: '.header',
    el3: '.footer',
    loggedIn: false,

    events: {
      "keyPress #loginForm input" : "keyPress",
      "click #submitLogin" : "validate",
      "submit #dbForm"    : "goTo",
      "click #logout"     : "logout",
      "change #loginDatabase" : "renderDBS"
    },

    template: templateEngine.createTemplate("loginView.ejs"),

    render: function(loggedIn) {
      var self = this;

      $(this.el).html(this.template.render({}));
      $(this.el2).hide();
      $(this.el3).hide();

      if (frontendConfig.authenticationEnabled && loggedIn !== true) {
        window.setTimeout(function() {
          $('#loginUsername').focus();
        }, 300);
      }
      else {
        var url = arangoHelper.databaseUrl("/_api/database/user");

        if (frontendConfig.authenticationEnabled === false) {
          $('#logout').hide();
          $('.login-window #databases').css('height', '90px');
        }

        $('#loginForm').hide();
        $('.login-window #databases').show();

        $.ajax(url).success(function(data) {
          //enable db select and login button
          $('#loginDatabase').html('');
          //fill select with allowed dbs
          _.each(data.result, function(db) {
            $('#loginDatabase').append(
              '<option>' + db + '</option>'
            ); 
          });

          self.renderDBS();
        });
      }

      $('.bodyWrapper').show();

      return this;
    },

    clear: function () {
      $('#loginForm input').removeClass("form-error");
      $('.wrong-credentials').hide();
    },

    keyPress: function(e) {
      if (e.ctrlKey && e.keyCode === 13) {
        e.preventDefault();
        this.validate();
      }
      else if (e.metaKey && e.keyCode === 13) {
        e.preventDefault();
        this.validate();
      }
    },

    validate: function(event) {
      event.preventDefault();
      this.clear();

      var username = $('#loginUsername').val();
      var password = $('#loginPassword').val();

      if (!username) {
        //do not send unneccessary requests if no user is given
        return;
      }

      var callback = function(error) {
        var self = this;
        if (error) {
          $('.wrong-credentials').show();
          $('#loginDatabase').html('');
          $('#loginDatabase').append(
            '<option>_system</option>'
          ); 
        }
        else {
          var url = arangoHelper.databaseUrl("/_api/database/user", '_system');

          if (frontendConfig.authenticationEnabled === false) {
            url = arangoHelper.databaseUrl("/_api/database/user");
          }

          $('.wrong-credentials').hide();
          self.loggedIn = true;
          //get list of allowed dbs
          $.ajax(url).success(function(data) {

            $('#loginForm').hide();
            $('#databases').show();

            //enable db select and login button
            $('#loginDatabase').html('');
            //fill select with allowed dbs
            _.each(data.result, function(db) {
              $('#loginDatabase').append(
                '<option>' + db + '</option>'
              ); 
            });

            self.renderDBS();
          });
        }
      }.bind(this);

      this.collection.login(username, password, callback);
    },

    renderDBS: function() {
      var db = $('#loginDatabase').val();
      $('#goToDatabase').html("Select: "  + db);
      window.setInterval(function() {
        $('#goToDatabase').focus();
      }, 300);
    },

    logout: function() {
      this.collection.logout();
    },

    goTo: function (e) {
      e.preventDefault();
      var username = $('#loginUsername').val();
      var database = $('#loginDatabase').val();
      console.log(window.App.dbSet);
      window.App.dbSet = database;
      console.log(window.App.dbSet);

      var callback2 = function(error) {
        if (error) {
          arangoHelper.arangoError("User", "Could not fetch user settings"); 
        }
      };

      var path = window.location.protocol + "//" + window.location.host
                 + frontendConfig.basePath + "/_db/" + database + "/_admin/aardvark/index.html";

      window.location.href = path;

      //show hidden divs
      $(this.el2).show();
      $(this.el3).show();
      $('.bodyWrapper').show();
      $('.navbar').show();

      $('#currentUser').text(username);
      this.collection.loadUserSettings(callback2);
    }

  });
}());
