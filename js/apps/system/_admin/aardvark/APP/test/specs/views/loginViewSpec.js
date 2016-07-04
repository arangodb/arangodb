/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/* global window, document, hljs, $*/

(function () {
  'use strict';

  describe('The loginView', function () {
    var view;

    beforeEach(function () {
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        }
      };

      view = new window.loginView();

      expect(view.events).toEqual({
        'click #submitLogin': 'login',
        'keydown #loginUsername': 'checkKey',
        'keydown #loginPassword': 'checkKey'
      });
    });

    afterEach(function () {
      delete window.App;
    });

    it('should render', function () {
      view.template = {
        render: function () {}
      };
      var collectionDummy = {
          add: function () {}
        }, jqueryDummy = {
          html: function () {},
          hide: function () {},
          focus: function () {},
          val: function () {}

      };
      view.collection = collectionDummy;

      spyOn(view.template, 'render').andReturn(1);

      spyOn(collectionDummy, 'add');
      spyOn(jqueryDummy, 'html');
      spyOn(jqueryDummy, 'hide');
      spyOn(jqueryDummy, 'focus');
      spyOn(jqueryDummy, 'val');
      spyOn(view, 'addDummyUser').andCallThrough();
      spyOn(window, '$').andReturn(jqueryDummy);

      view.render();

      expect(view.collection.add).toHaveBeenCalledWith({
        'userName': 'admin',
        'sessionId': 'abc123',
        'password': 'admin',
        'userId': 1
      });

      expect(window.$).toHaveBeenCalledWith('.header');
      expect(window.$).toHaveBeenCalledWith('.footer');
      expect(window.$).toHaveBeenCalledWith('#loginUsername');
      expect(window.$).toHaveBeenCalledWith('#loginPassword');

      expect(jqueryDummy.html).toHaveBeenCalledWith(1);
      expect(jqueryDummy.hide).toHaveBeenCalled();
      expect(jqueryDummy.focus).toHaveBeenCalled();
      expect(jqueryDummy.val).toHaveBeenCalledWith('admin');

      expect(view.addDummyUser).toHaveBeenCalled();
      expect(view.template.render).toHaveBeenCalledWith({});
    });

    it('should checkKey', function () {
      spyOn(view, 'login');
      view.checkKey({keyCode: 13});
      expect(view.login).toHaveBeenCalled();
    });

    it('should checkKey with invalid key', function () {
      spyOn(view, 'login');
      view.checkKey({keyCode: 12});
      expect(view.login).not.toHaveBeenCalled();
    });

    it('should login', function () {
      var collectionDummy = {
          login: function () {},
          loadUserSettings: function () {}
        }, jqueryDummy = {
          show: function () {},
          text: function () {},
          val: function () {}

      };
      view.collection = collectionDummy;

      spyOn(collectionDummy, 'login').andReturn(true);
      spyOn(collectionDummy, 'loadUserSettings');
      spyOn(jqueryDummy, 'show');
      spyOn(jqueryDummy, 'text');
      spyOn(jqueryDummy, 'val').andReturn('someData');
      spyOn(window, '$').andReturn(jqueryDummy);
      spyOn(window.App, 'navigate');

      view.login();

      expect(view.collection.login).toHaveBeenCalledWith('someData', 'someData');
      expect(view.collection.loadUserSettings).toHaveBeenCalled();

      expect(window.$).toHaveBeenCalledWith('.header');
      expect(window.$).toHaveBeenCalledWith('.footer');
      expect(window.$).toHaveBeenCalledWith('#loginUsername');
      expect(window.$).toHaveBeenCalledWith('#loginPassword');
      expect(window.$).toHaveBeenCalledWith('#currentUser');

      expect(window.App.navigate).toHaveBeenCalledWith('/', {trigger: true});

      expect(jqueryDummy.show).toHaveBeenCalled();
      expect(jqueryDummy.text).toHaveBeenCalledWith('someData');
      expect(jqueryDummy.val).toHaveBeenCalled();
    });

    it('should login but fail', function () {
      var collectionDummy = {
          login: function () {},
          loadUserSettings: function () {}
        }, jqueryDummy = {
          show: function () {},
          text: function () {},
          val: function () {}

      };
      view.collection = collectionDummy;

      spyOn(collectionDummy, 'login').andReturn(false);
      spyOn(collectionDummy, 'loadUserSettings');
      spyOn(jqueryDummy, 'show');
      spyOn(jqueryDummy, 'text');
      spyOn(jqueryDummy, 'val').andReturn('someData');
      spyOn(window, '$').andReturn(jqueryDummy);
      spyOn(window.App, 'navigate');

      view.login();

      expect(view.collection.login).toHaveBeenCalledWith('someData', 'someData');
      expect(view.collection.loadUserSettings).not.toHaveBeenCalled();

      expect(window.$).toHaveBeenCalledWith('#loginUsername');
      expect(window.$).toHaveBeenCalledWith('#loginPassword');

      expect(window.App.navigate).not.toHaveBeenCalled();

      expect(jqueryDummy.show).not.toHaveBeenCalled();
      expect(jqueryDummy.text).not.toHaveBeenCalled();
      expect(jqueryDummy.val).toHaveBeenCalled();
    });

    it('should not login due to mnissing data', function () {
      var collectionDummy = {
          login: function () {},
          loadUserSettings: function () {}
        }, jqueryDummy = {
          show: function () {},
          text: function () {},
          val: function () {}

      };
      view.collection = collectionDummy;

      spyOn(collectionDummy, 'login').andReturn(false);
      spyOn(collectionDummy, 'loadUserSettings');
      spyOn(jqueryDummy, 'show');
      spyOn(jqueryDummy, 'text');
      spyOn(jqueryDummy, 'val').andReturn('');
      spyOn(window, '$').andReturn(jqueryDummy);
      spyOn(window.App, 'navigate');

      view.login();

      expect(view.collection.login).not.toHaveBeenCalled();
      expect(view.collection.loadUserSettings).not.toHaveBeenCalled();

      expect(window.$).toHaveBeenCalledWith('#loginUsername');
      expect(window.$).toHaveBeenCalledWith('#loginPassword');

      expect(window.App.navigate).not.toHaveBeenCalled();

      expect(jqueryDummy.show).not.toHaveBeenCalled();
      expect(jqueryDummy.text).not.toHaveBeenCalled();
      expect(jqueryDummy.val).toHaveBeenCalled();
    });
  });
}());
