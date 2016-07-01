/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jQuery*/
/* global runs, waitsFor, jasmine, waits*/
/* global $, console, arangoHelper */
(function () {
  'use strict';
  describe('User Management View', function () {
    var myStore,
      isCoordinator,
      myView,
      div,
      div2;

    beforeEach(function () {
      isCoordinator = false;

      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);

      div2 = document.createElement('div');
      div2.id = 'modalPlaceholder';
      document.body.appendChild(div2);

      window.App = {
        notificationList: {
          add: function () {
            throw 'Should be a spy';
          }
        }
      };

      spyOn(window.App.notificationList, 'add');

      myStore = new window.ArangoUsers();

      myView = {
        collection: myStore
      };
      spyOn(myView.collection, 'fetch').andReturn({});

      myView = new window.userManagementView({
        collection: myStore
      });
      myView.render();

      window.modalView = new window.ModalView();
    });

    afterEach(function () {
      delete window.App;
      delete window.modalView;
      document.body.removeChild(div);
      document.body.removeChild(div2);
    });

    describe('user management view', function () {
      it('assert the basics', function () {
        var myEvents = {
          'click #createUser': 'createUser',
          'click #submitCreateUser': 'submitCreateUser',
          'click .editUser': 'editUser',
          'click .icon': 'editUser',
          'click #submitEditUser': 'submitEditUser',
          'click #userManagementToggle': 'toggleView',
          'keyup #userManagementSearchInput': 'search',
          'click #userManagementSearchSubmit': 'search',
          'click #callEditUserPassword': 'editUserPassword',
          'click #submitEditUserPassword': 'submitEditUserPassword',
          'click #submitEditCurrentUserProfile': 'submitEditCurrentUserProfile',
          'click .css-label': 'checkBoxes',
          'change #userSortDesc': 'sorting'
        };
        expect(myEvents).toEqual(myView.events);
      });

      it('should check the render function (with visible dropdown)', function () {
        $('#userManagementDropdown').show();
        spyOn($.fn, 'show');
        myView.render();
        expect($.fn.show).toHaveBeenCalled();
      });

      it('should render user modal view', function () {
        spyOn(myView, 'editCurrentUser');
        myView.render('DummyUser');
        expect(myView.editCurrentUser).toHaveBeenCalled();
      });

      it('should fire the event for creating a user', function () {
        spyOn(myView, 'createCreateUserModal');

        var e = {
          preventDefault: function () {}
        };

        spyOn(e, 'preventDefault');
        myView.createUser(e);
        expect(e.preventDefault).toHaveBeenCalled();
        expect(myView.createCreateUserModal).toHaveBeenCalled();
      });

      it('should submit creation of a new user', function () {
        spyOn(myView.collection, 'create').andCallFake(function (options, options2) {
          options2.success();
        });

        spyOn(window.modalView, 'hide');
        spyOn(myView, 'updateUserManagement');

        var name = 'Dummy User', options = {
            user: 'dummyuser',
            passwd: 'dummypassword',
            active: false,
            extra: {name: name}
        };

        myView.createCreateUserModal();
        $('#newUsername').val(options.user);
        $('#newName').val(name);
        $('#newPassword').val(options.passwd);
        $('#newStatus').click();
        myView.submitCreateUser();

        expect(myView.collection.create).toHaveBeenCalledWith(options, {
          wait: true, error: jasmine.any(Function), success: jasmine.any(Function)
        });

        expect(window.modalView.hide).toHaveBeenCalled();
        expect(myView.updateUserManagement).toHaveBeenCalled();
      });

      it('should cancel the submit creation of a new user', function () {
        spyOn(myView.collection, 'create');

        var name = 'Dummy User', options = {
            user: '',
            passwd: 'dummypassword',
            active: false,
            extra: {name: name}
        };

        myView.createCreateUserModal();
        $('#newUsername').val(options.user);
        $('#newName').val(name);
        $('#newPassword').val(options.passwd);
        $('#newStatus').click();
        myView.submitCreateUser();

        expect(myView.collection.create).not.toHaveBeenCalled();
      });
    });
  });
}());
