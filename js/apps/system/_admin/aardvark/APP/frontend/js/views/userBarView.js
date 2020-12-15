/* jshint browser: true */
/* jshint unused: false */
/* global frontendConfig, arangoHelper, Backbone, templateEngine, $, window */
(function () {
  'use strict';

  window.UserBarView = Backbone.View.extend({
    events: {
      'change #userBarSelect': 'navigateBySelect',
      'click .tab': 'navigateByTab',
      'mouseenter .dropdown': 'showDropdown',
      'mouseleave .dropdown': 'hideDropdown',
      'click #userLogoutIcon': 'userLogout',
      'click #userLogout': 'userLogout'
    },

    initialize: function (options) {
      // set user ro/rw mode
      arangoHelper.checkDatabasePermissions(this.setUserCollectionMode.bind(this));
      this.userCollection = options.userCollection;
      this.userCollection.fetch({
        cache: false,
        async: true
      });
      this.userCollection.bind('change:extra', this.render.bind(this));
    },

    setUserCollectionMode: function (readOnly) {
      if (readOnly) {
        this.userCollection.authOptions.ro = true;
      }
    },

    template: templateEngine.createTemplate('userBarView.ejs'),

    navigateBySelect: function () {
      var navigateTo = $('#arangoCollectionSelect').find('option:selected').val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).closest('a');
      var navigateTo = tab.attr('id');
      if (navigateTo === 'user') {
        $('#user_dropdown').slideToggle(200);
        e.preventDefault();
        return;
      }
      window.App.navigate(navigateTo, {trigger: true});
      e.preventDefault();
    },

    toggleUserMenu: function () {
      $('#userBar .subBarDropdown').toggle();
    },

    showDropdown: function () {
      $('#user_dropdown').fadeIn(1);
    },

    hideDropdown: function () {
      $('#user_dropdown').fadeOut(1);
    },

    render: function () {
      if (frontendConfig.authenticationEnabled === false) {
        return;
      }

      var self = this;

      var callback = function (error, username) {
        if (error) {
          arangoHelper.arangoErro('User', 'Could not fetch user.');
        } else {
          var img = null;
          var name = null;
          var active = false;
          var currentUser = null;
          if (username !== false) {
            var continueFunc = function () {
              if (frontendConfig.ldapEnabled) {
                self.userCollection.add({
                  name: window.App.currentUser,
                  user: window.App.currentUser,
                  username: window.App.currentUser,
                  active: true,
                  img: undefined
                });
              }
              currentUser = self.userCollection.findWhere({user: username});
              currentUser.set({loggedIn: true});
              name = currentUser.get('extra').name;
              img = currentUser.get('extra').img;
              active = currentUser.get('active');
              if (!img) {
                img = 'img/default_user.png';
              } else {
                img = 'https://s.gravatar.com/avatar/' + img + '?s=80';
              }
              if (!name) {
                name = '';
              }

              self.$el = $('#userBar');
              self.$el.html(self.template.render({
                img: img,
                name: name,
                username: username,
                active: active
              }));

              self.delegateEvents();
              return self.$el;
            };
            if (self.userCollection.models.length === 0) {
              self.userCollection.fetch({
                success: function () {
                  continueFunc(error, username);
                }
              });
            } else {
              continueFunc(error, username);
            }
          }
        }
      };

      $('#userBar').on('click', function () {
        self.toggleUserMenu();
      });

      this.userCollection.whoAmI(callback);
    },

    userLogout: function () {
      var userCallback = function (error) {
        if (error) {
          arangoHelper.arangoError('User', 'Logout error');
        } else {
          this.userCollection.logout();
        }
      }.bind(this);
      this.userCollection.whoAmI(userCallback);
    }
  });
}());
