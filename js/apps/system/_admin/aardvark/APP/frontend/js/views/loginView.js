/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, location, document, _, arangoHelper, window, $, templateEngine, frontendConfig */

(function () {
  'use strict';
  window.LoginView = Backbone.View.extend({
    el: '#content',
    loggedIn: false,
    loginCounter: 0,

    events: {
      'keyPress #loginForm input': 'keyPress',
      'click #submitLogin': 'validate',
      'submit #dbForm': 'goTo',
      'click #logout': 'logout',
      'change #loginDatabase': 'renderDBS',
      'keyup #databaseInputName': 'renderDBS'
    },

    template: templateEngine.createTemplate('loginView.ejs'),

    render: function (loggedIn) {
      $(this.el).html(this.template.render({}));

      if (frontendConfig.isEnterprise) {
        $('#ArangoDBLogoVersion').text('Enterprise Edition');
        $('#ArangoDBLogoVersion').removeClass('enterprise');
      } else {
        $('#ArangoDBLogoVersion').text('Community Edition');
        $('#ArangoDBLogoVersion').removeClass('community');
      }

      let user = arangoHelper.getCurrentJwtUsername();
      if (!user || user === "undefined") user = null;
      if (frontendConfig.authenticationEnabled && !loggedIn && user === null) {
        $('.bodyWrapper').show();
        setTimeout(() => {
          $('#loginUsername').focus();
        }, 300);
      } else {
        let errCallback = () => {
          // existing jwt login is not valid anymore => reload
          arangoHelper.setCurrentJwt(null, null);
          location.reload(true);
        };
        if (frontendConfig.authenticationEnabled && !loggedIn) {
          // try if existent jwt is valid
          errCallback = () => {
            this.collection.logout();
            $('.bodyWrapper').show();
            setTimeout(() => {
              $('#loginUsername').focus();
            }, 300);
          };
        } else if (!frontendConfig.authenticationEnabled || !loggedIn) {
          user = null;
        }

        if (!frontendConfig.authenticationEnabled) {
          $('#logout').hide();
          $('.login-window #databases').css('height', '90px');
        }

        $('#loginForm').hide();
        $('.login-window #databases').show();

        $.ajax({
          type: "GET",
          url: user
            ? arangoHelper.databaseUrl(`/_api/user/${encodeURIComponent(user)}/database`, '_system')
            : arangoHelper.databaseUrl('/_api/database/user'),
          success: (permissions) => {
            let dbs = permissions.result;
            if (user) {
              // key, value tuples of database name and permission
              dbs = Object.keys(dbs);
            }
            // enable db select and login button
            this.renderDatabasesDropdown(dbs);
            this.renderDBS();
            $('.bodyWrapper').show();
          },
          error: (e) => {
            errCallback();
          }
        });
      }

      return this;
    },

    sortDatabases: function (obj) {
      if (frontendConfig.authenticationEnabled) {
        // key, value tuples of database name and permission
        obj = Object.keys(obj);
      } else {
        // array contained only the database names
      }
      return _.sortBy(obj, (value) => value.toLowerCase());
    },

    clear: function () {
      $('#loginForm input').removeClass('form-error');
      $('.wrong-credentials').hide();
    },

    keyPress: function (evt) {
      if (evt.ctrlKey && evt.keyCode === 13) {
        evt.preventDefault();
        this.validate();
      } else if (evt.metaKey && evt.keyCode === 13) {
        evt.preventDefault();
        this.validate();
      }
    },

    validate: function (event) {
      event.preventDefault();
      this.clear();

      const username = $('#loginUsername').val();
      const password = $('#loginPassword').val();

      if (!username) {
        // do not send unnecessary requests if no user is given
        return;
      }

      this.collection.login(username, password, () => this.loginCallback(username, password));
    },

    loginCallback: function (username, password, error) {
      if (error) {
        if (this.loginCounter === 0) {
          this.loginCounter++;
          this.collection.login(username, password, () => this.loginCallback(username));
          return;
        }
        this.loginCounter = 0;
        $('.wrong-credentials').show();
        $('#loginDatabase').html('');
        $('#loginDatabase').append(
          '<option>_system</option>'
        );
      } else {
        this.renderDBSelection(username);
      }
    },

    renderDBSelection: function (username) {
      $('.wrong-credentials').hide();
      this.loggedIn = true;

      // get list of allowed dbs
      $.ajax({
        type: "GET",
        url: frontendConfig.authenticationEnabled
          ? arangoHelper.databaseUrl(`/_api/user/${encodeURIComponent(username)}/database`, '_system')
          : arangoHelper.databaseUrl('/_api/database/user'),
        success: (permissions) => {
          $('#loginForm').hide();
          $('.login-window #databases').show();

          // enable db select and login button
          let dbs = permissions.result;
          if (frontendConfig.authenticationEnabled) {
            // key, value tuples of database name and permission
            dbs = Object.keys(dbs);
          }
          this.renderDatabasesDropdown(dbs);
          this.renderDBS();
        },
        error: () => {
          $('.wrong-credentials').show();
        }
      });
    },

    renderDatabasesDropdown: function (dbs) {
      $('#loginDatabase').html('');

      if (dbs.length > 0) {
        // show select, remove input
        $('#databaseInputName').remove();

        for (const db of _.sortBy(dbs, (db) => db.toLowerCase())) {
          $('#loginDatabase').append(
            '<option value="' + _.escape(db) + '"><pre>' + _.escape(db) + '</pre></option>'
          );
        }
            
        $('#loginDatabase').show();
        $('.fa-database').show();
      } else {
        $('#loginDatabase').hide();
        $('.fa-database').hide();
        $('#loginDatabase').after(
          '<input id="databaseInputName" class="databaseInput login-input" placeholder="_system" value="_system"></input>'
        );
      }
    },

    renderDBS: function () {
      let message = 'Select DB: ';

      $('#noAccess').hide();
      if ($('#loginDatabase').children().length === 0) {
        if ($('#loginDatabase').is(':visible')) {
          $('#dbForm').remove();
          $('.login-window #databases').prepend(
            '<div class="no-database">You do not have permission to a database.</div>'
          );
          message = message + $('#loginDatabase').val();
          window.setTimeout(function () {
            $('#goToDatabase').focus();
          }, 150);
        } else {
          message = message + $('#databaseInputName').val();
        }
      } else {
        message = message + $('#loginDatabase').val();
        window.setTimeout(function () {
          $('#goToDatabase').focus();
        }, 150);
      }
      $('#goToDatabase').html(_.escape(message));
    },

    logout: function () {
      this.collection.logout();
    },

    goTo: function (evt) {
      evt.preventDefault();
      const username = $('#loginUsername').val();
      let database;

      if ($('#databaseInputName').is(':visible')) {
        database = $('#databaseInputName').val();
      } else {
        database = $('#loginDatabase').val();
      }

      let path = window.location.protocol + '//' + window.location.host +
        frontendConfig.basePath + '/_db/' + encodeURIComponent(database) + '/_admin/aardvark/index.html';

      $.ajax({
        url: path,
        success: (data) => {
          window.location.href = path;
  
          // show hidden divs
          $('.bodyWrapper').show();
          $('.navbar').show();
  
          $('#currentUser').text(username);
          this.collection.loadUserSettings((error) => {
            if (error) {
              arangoHelper.arangoError('User', 'Could not fetch user settings');
            }
          });
        },
        error: (data) => {
          if (data.responseJSON && data.responseJSON.errorMessage) {
            $('#noAccess').html('Error (DB: ' + _.escape(database) + '): ' + _.escape(data.responseJSON.errorMessage));
          } else {
            $('#noAccess').html('Error (DB: ' + _.escape(database) + '): ' + _.escape(data.statusText));
          }
          $('#noAccess').show();
        }
      });
    }

  });
}());
