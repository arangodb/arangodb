/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, location, document, _, arangoHelper, window, $, templateEngine, frontendConfig */

(function () {
  'use strict';
  window.LoginView = Backbone.View.extend({
    el: '#content',
    el2: '.header',
    el3: '.footer',
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
      var self = this;
      $(this.el).html(this.template.render({}));
      $(this.el2).hide();
      $(this.el3).hide();

      var continueRender = function (user, errCallback, debug) {
        var url;

        if (!user) {
          url = arangoHelper.databaseUrl('/_api/database/user');
        } else {
          url = arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(user) + '/database', '_system');
        }

        if (frontendConfig.authenticationEnabled === false) {
          $('#logout').hide();
          $('.login-window #databases').css('height', '90px');
        }

        $('#loginForm').hide();
        $('.login-window #databases').show();

        $.ajax({
          type: "GET",
          url: url,
          success: function (permissions) {
            //  enable db select and login button
            $('#loginDatabase').html('');
            // fill select with allowed dbs
            if (Object.keys(permissions.result).length > 0) {
              // show select, remove input
              $('#loginDatabase').show();
              $('.fa-database').show();
              $('#databaseInputName').remove();

              var sortedObj = self.sortDatabases(permissions.result);
              _.each(sortedObj, function (rule, db) {
                if (frontendConfig.authenticationEnabled) {
                  $('#loginDatabase').append(
                      '<option>' + db + '</option>'
                  );
                } else {
                  $('#loginDatabase').append(
                      '<option>' + rule + '</option>'
                  );
                }
              });
            } else {
              $('#loginDatabase').hide();
              $('.fa-database').hide();
              $('#loginDatabase').after(
                  '<input id="databaseInputName" class="databaseInput login-input" placeholder="_system" value="_system"></input>'
              );
            }

            self.renderDBS();
          },
          error: function (e) {
            if (errCallback) {
              errCallback();
            } else {
              // existing jwt login is not valid anymore => reload
              arangoHelper.setCurrentJwt(null, null);
              location.reload(true);
            }
          }
        });
      };

      if (frontendConfig.authenticationEnabled && loggedIn !== true) {
        var usr = arangoHelper.getCurrentJwtUsername();
        if (usr !== null && usr !== 'undefined' && usr !== undefined) {
          // try if existent jwt is valid
          var errCallback = function () {
            self.collection.logout();
            window.setTimeout(function () {
              $('#loginUsername').focus();
            }, 300);
          };
          continueRender(arangoHelper.getCurrentJwtUsername(), errCallback);
        } else {
          window.setTimeout(function () {
            $('#loginUsername').focus();
          }, 300);
        }
      } else if (frontendConfig.authenticationEnabled && loggedIn) {
        continueRender(arangoHelper.getCurrentJwtUsername(), null, 4);
      } else {
        continueRender(null, null, 3);
      }

      $('.bodyWrapper').show();
      this.setVersion();

      return this;
    },

    setVersion: function () {
      if (window.frontendConfig && window.frontendConfig.isEnterprise) {
        $('#ArangoDBLogoVersion').attr('src', 'img/ArangoDB-enterprise-edition-Web-UI.png');
      } else {
        $('#ArangoDBLogoVersion').attr('src', 'img/ArangoDB-community-edition-Web-UI.png');
      }
    },

    sortDatabases: function (obj) {
      var sorted;

      if (frontendConfig.authenticationEnabled) {
        // key, value tuples of database name and permission
        sorted = _.pairs(obj);
        sorted = _.sortBy(sorted, function (i) { return i[0].toLowerCase(); });
        sorted = _.object(sorted);
      } else {
        // array contained only the database names
        sorted = _.sortBy(obj, function (i) { return i.toLowerCase(); });
      }
      return sorted;
    },

    clear: function () {
      $('#loginForm input').removeClass('form-error');
      $('.wrong-credentials').hide();
    },

    keyPress: function (e) {
      if (e.ctrlKey && e.keyCode === 13) {
        e.preventDefault();
        this.validate();
      } else if (e.metaKey && e.keyCode === 13) {
        e.preventDefault();
        this.validate();
      }
    },

    validate: function (event) {
      event.preventDefault();
      this.clear();

      var username = $('#loginUsername').val();
      var password = $('#loginPassword').val();

      if (!username) {
        // do not send unneccessary requests if no user is given
        return;
      }

      this.collection.login(username, password, this.loginCallback.bind(this, username, password));
    },

    loginCallback: function (username, password, error) {
      var self = this;

      if (error) {
        if (self.loginCounter === 0) {
          self.loginCounter++;
          self.collection.login(username, password, this.loginCallback.bind(this, username));
          return;
        }
        self.loginCounter = 0;
        $('.wrong-credentials').show();
        $('#loginDatabase').html('');
        $('#loginDatabase').append(
          '<option>_system</option>'
        );
      } else {
        self.renderDBSelection(username);
      }
    },

    renderDBSelection: function (username) {
      var self = this;
      var url = arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(username) + '/database', '_system');

      if (frontendConfig.authenticationEnabled === false) {
        url = arangoHelper.databaseUrl('/_api/database/user');
      }

      $('.wrong-credentials').hide();
      self.loggedIn = true;

      // get list of allowed dbs
      $.ajax({
        type: "GET",
        url: url,
        success: function (permissions) {
          $('#loginForm').hide();
          $('.login-window #databases').show();

          // enable db select and login button
          $('#loginDatabase').html('');

          if (Object.keys(permissions.result).length > 0) {
            // show select, remove input
            $('#loginDatabase').show();
            $('.fa-database').show();
            $('#databaseInputName').remove();

            var sortedObj = self.sortDatabases(permissions.result);
            _.each(sortedObj, function (rule, db) {
              if (frontendConfig.authenticationEnabled) {
                $('#loginDatabase').append(
                    '<option>' + db + '</option>'
                );
              } else {
                $('#loginDatabase').append(
                    '<option>' + rule + '</option>'
                );
              }
            });
          } else {
            $('#loginDatabase').hide();
            $('.fa-database').hide();
            $('#loginDatabase').after(
                '<input id="databaseInputName" class="databaseInput login-input" placeholder="_system" value="_system"></input>'
            );
          }

          self.renderDBS();
        },
        error: function () {
          $('.wrong-credentials').show();
        }
      });
    },

    renderDBS: function () {
      var message = 'Select DB: ';

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
      $('#goToDatabase').html(message);
    },

    logout: function () {
      this.collection.logout();
    },

    goTo: function (e) {
      e.preventDefault();
      var username = $('#loginUsername').val();
      var database;

      if ($('#databaseInputName').is(':visible')) {
        database = $('#databaseInputName').val();
      } else {
        database = $('#loginDatabase').val();
      }
      window.App.dbSet = database;

      var callback2 = function (error) {
        if (error) {
          arangoHelper.arangoError('User', 'Could not fetch user settings');
        }
      };

      var path = window.location.protocol + '//' + window.location.host +
        frontendConfig.basePath + '/_db/' + database + '/_admin/aardvark/index.html';
      if (frontendConfig.react) {
        path = window.location.protocol + '//' + window.location.host +
          '/_db/' + database + '/_admin/aardvark/index.html';
      }

      var continueFunction = function () {
        window.location.href = path;

        // show hidden divs
        $(this.el2).show();
        $(this.el3).show();
        $('.bodyWrapper').show();
        $('.navbar').show();

        $('#currentUser').text(username);
        this.collection.loadUserSettings(callback2);
      }.bind(this);

      $.ajax({
        url: path,
        success: function (data) {
          continueFunction();
        },
        error: function (data) {
          if (data.responseJSON && data.responseJSON.errorMessage) {
            $('#noAccess').html('Error (DB: ' + database + '): ' + data.responseJSON.errorMessage);
          } else {
            $('#noAccess').html('Error (DB: ' + database + '): ' + data.statusText);
          }
          $('#noAccess').show();
        }
      });
    }

  });
}());
