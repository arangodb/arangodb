/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, document, _, arangoHelper, window, setTimeout, $, templateEngine, frontendConfig */

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
      'change #loginDatabase': 'renderDBS'
    },

    template: templateEngine.createTemplate('loginView.ejs'),

    render: function (loggedIn) {
      var self = this;
      $(this.el).html(this.template.render({}));
      $(this.el2).hide();
      $(this.el3).hide();

      var continueRender = function (user, errCallback) {
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

        $.ajax(url).success(function (permissions) {
          var successFunc = function (availableDbs) {
            //  enable db select and login button
            $('#loginDatabase').html('');
            // fill select with allowed dbs
            _.each(permissions.result, function (rule, db) {
              if (errCallback) {
                if (availableDbs) {
                  if (availableDbs.indexOf(db) > -1) {
                    $('#loginDatabase').append(
                      '<option>' + db + '</option>'
                    );
                  }
                } else {
                  $('#loginDatabase').append(
                    '<option>' + db + '</option>'
                  );
                }
              } else {
                if (availableDbs) {
                  if (availableDbs.indexOf(rule) > -1) {
                    $('#loginDatabase').append(
                      '<option>' + rule + '</option>'
                    );
                  }
                } else {
                  $('#loginDatabase').append(
                    '<option>' + rule + '</option>'
                  );
                }
              }
            });

            self.renderDBS();
          };

          // fetch available dbs
          var availableDbs;
          try {
            $.ajax(arangoHelper.databaseUrl('/_api/database/user')).success(function (dbs) {
              availableDbs = dbs.result;
              successFunc(availableDbs);
            });
          } catch (ignore) {
            successFunc();
          }
        }).error(function () {
          if (errCallback) {
            errCallback();
          } else {
            console.log('could not fetch user db data');
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
      } else {
        continueRender();
      }

      $('.bodyWrapper').show();
      self.checkVersion();

      return this;
    },

    checkVersion: function () {
      var self = this;
      window.setTimeout(function () {
        var a = document.getElementById('loginSVG');
        var svgDoc = a.contentDocument;
        var svgItem;

        if (frontendConfig.isEnterprise !== undefined) {
          if (frontendConfig.isEnterprise) {
            svgItem = svgDoc.getElementById('logo-enterprise');
          } else {
            svgItem = svgDoc.getElementById('logo-community');
          }
          svgItem.setAttribute('visibility', 'visible');
        } else {
          self.checkVersion();
        }
      }, 150);
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
      $.ajax(url).success(function (permissions) {
        // HANDLE PERMISSIONS
        _.each(permissions.result, function (value, key) {
          if (value !== 'rw') {
            delete permissions.result[key];
          }
        });

        $('#loginForm').hide();
        $('.login-window #databases').show();

        // enable db select and login button
        $('#loginDatabase').html('');

        var successFunc = function (availableDbs) {
          if (availableDbs) {
            _.each(permissions.result, function (db, key) {
              if (availableDbs.indexOf(key) > -1) {
                $('#loginDatabase').append(
                  '<option>' + key + '</option>'
                );
              }
            });
          } else {
            // fill select with allowed dbs
            _.each(permissions.result, function (db, key) {
              $('#loginDatabase').append(
                '<option>' + key + '</option>'
              );
            });
          }

          self.renderDBS();
        };

        // fetch available dbs
        try {
          $.ajax(arangoHelper.databaseUrl('/_api/database/user')).success(function (dbs) {
            successFunc(dbs.result);
          });
        } catch (ignore) {
          successFunc();
        }
      }).error(function () {
        $('.wrong-credentials').show();
      });
    },

    renderDBS: function () {
      if ($('#loginDatabase').children().length === 0) {
        $('#dbForm').remove();
        $('.login-window #databases').prepend(
          '<div class="no-database">You do not have permission to a database.</div>'
        );
      } else {
        var db = $('#loginDatabase').val();
        $('#goToDatabase').html('Select DB: ' + db);
        window.setTimeout(function () {
          $('#goToDatabase').focus();
        }, 300);
      }
    },

    logout: function () {
      this.collection.logout();
    },

    goTo: function (e) {
      e.preventDefault();
      var username = $('#loginUsername').val();
      var database = $('#loginDatabase').val();
      window.App.dbSet = database;

      var callback2 = function (error) {
        if (error) {
          arangoHelper.arangoError('User', 'Could not fetch user settings');
        }
      };

      var path = window.location.protocol + '//' + window.location.host +
        frontendConfig.basePath + '/_db/' + database + '/_admin/aardvark/index.html';

      window.location.href = path;

      // show hidden divs
      $(this.el2).show();
      $(this.el3).show();
      $('.bodyWrapper').show();
      $('.navbar').show();

      $('#currentUser').text(username);
      this.collection.loadUserSettings(callback2);
    }

  });
}());
