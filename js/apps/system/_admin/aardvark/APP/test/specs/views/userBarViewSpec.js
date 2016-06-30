/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/* global window, document, hljs, $*/

(function () {
  'use strict';

  describe('The loginView', function () {
    var view, collectionDummy;

    beforeEach(function () {
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        }
      };

      collectionDummy = {
        fetch: function () {},
        bind: function () {},
        whoAmI: function () {},
        logout: function () {},
        findWhere: function () {}

      };

      spyOn(collectionDummy, 'fetch');
      spyOn(collectionDummy, 'bind');

      view = new window.UserBarView({userCollection: collectionDummy});

      expect(collectionDummy.fetch).toHaveBeenCalledWith({async: false});
      expect(collectionDummy.bind).toHaveBeenCalled();

      expect(view.events).toEqual({
        'change #userBarSelect': 'navigateBySelect',
        'click .tab': 'navigateByTab',
        'mouseenter .dropdown': 'showDropdown',
        'mouseleave .dropdown': 'hideDropdown',
        'click #userLogout': 'userLogout'
      });
    });

    afterEach(function () {
      delete window.App;
    });

    it('should navigateBySelect', function () {
      var jquerDummy = {
        find: function () {
          return jquerDummy;
        },
        val: function () {
          return 'bla';
        }
      };
      spyOn(jquerDummy, 'find').andCallThrough();
      spyOn(jquerDummy, 'val').andCallThrough();
      spyOn(window, '$').andReturn(jquerDummy);
      spyOn(window.App, 'navigate');
      view.navigateBySelect();
      expect(window.$).toHaveBeenCalledWith('#arangoCollectionSelect');
      expect(jquerDummy.find).toHaveBeenCalledWith('option:selected');
      expect(jquerDummy.val).toHaveBeenCalled();
      expect(window.App.navigate).toHaveBeenCalledWith('bla', {trigger: true});
    });

    it('should navigateByTab', function () {
      var p = {
          srcElement: {id: 'bla'},
          preventDefault: function () {}
        },jqueryDummy = {
          closest: function () {
            return jqueryDummy;
          },
          attr: function () {},
          slideToggle: function () {}
      };

      spyOn(window, '$').andReturn(jqueryDummy);

      spyOn(jqueryDummy, 'closest').andCallThrough();
      spyOn(jqueryDummy, 'attr').andReturn('bla');

      spyOn(window.App, 'navigate');
      spyOn(p, 'preventDefault');
      view.navigateByTab(p);

      expect(window.$).toHaveBeenCalledWith({id: 'bla'});
      expect(jqueryDummy.closest).toHaveBeenCalledWith('a');
      expect(jqueryDummy.attr).toHaveBeenCalledWith('id');

      expect(p.preventDefault).toHaveBeenCalled();
      expect(window.App.navigate).toHaveBeenCalledWith('bla', {trigger: true});
    });

    it('should navigateByTab to links', function () {
      var p = {
          srcElement: {id: 'user'},
          preventDefault: function () {}
        },jqueryDummy = {
          closest: function () {
            return jqueryDummy;
          },
          attr: function () {},
          slideToggle: function () {}
      };

      spyOn(window, '$').andReturn(jqueryDummy);

      spyOn(jqueryDummy, 'closest').andCallThrough();
      spyOn(jqueryDummy, 'attr').andReturn('user');
      spyOn(jqueryDummy, 'slideToggle');

      spyOn(window.App, 'navigate');
      spyOn(p, 'preventDefault');
      view.navigateByTab(p);

      expect(window.$).toHaveBeenCalledWith({id: 'user'});
      expect(jqueryDummy.closest).toHaveBeenCalledWith('a');
      expect(jqueryDummy.slideToggle).toHaveBeenCalledWith(200);
      expect(jqueryDummy.attr).toHaveBeenCalledWith('id');

      expect(p.preventDefault).toHaveBeenCalled();
      expect(window.App.navigate).not.toHaveBeenCalled();
    });

    it('should navigateByTab to links', function () {
      var p = {
          srcElement: {id: 'user'}
        },jqueryDummy = {
          show: function () {}
      };

      spyOn(window, '$').andReturn(jqueryDummy);

      spyOn(jqueryDummy, 'show');

      view.showDropdown(p);

      expect(window.$).toHaveBeenCalledWith('#user_dropdown');
      expect(jqueryDummy.show).toHaveBeenCalledWith(200);
    });

    it('should hideDropdown', function () {
      var jqueryDummy = {
        hide: function () {}
      };

      spyOn(window, '$').andReturn(jqueryDummy);

      spyOn(jqueryDummy, 'hide');

      view.hideDropdown();

      expect(window.$).toHaveBeenCalledWith('#user_dropdown');
      expect(jqueryDummy.hide).toHaveBeenCalled();
    });

    it('should userLogout', function () {
      spyOn(collectionDummy, 'whoAmI');
      spyOn(collectionDummy, 'logout');

      view.userLogout();

      expect(collectionDummy.whoAmI).toHaveBeenCalled();
      expect(collectionDummy.logout).toHaveBeenCalled();
    });

    it('should render', function () {
      view.template = {
        render: function () {}
      };

      var userDummy = {
        set: function () {},
        get: function () {}
      };

      spyOn(collectionDummy, 'whoAmI').andReturn('userName');
      spyOn(collectionDummy, 'findWhere').andReturn(userDummy);
      spyOn(userDummy, 'set');
      spyOn(userDummy, 'get').andCallFake(function (a) {
        if (a === 'active') {
          return true;
        }
        return {
          img: undefined,
          name: undefined
        };
      });

      spyOn(view.template, 'render').andReturn(1);

      view.render();

      expect(userDummy.set).toHaveBeenCalledWith({loggedIn: true});
      expect(userDummy.get).toHaveBeenCalledWith('extra');
      expect(userDummy.get).toHaveBeenCalledWith('active');
      expect(collectionDummy.whoAmI).toHaveBeenCalled();
      expect(collectionDummy.findWhere).toHaveBeenCalledWith({user: 'userName'});
      expect(view.template.render).toHaveBeenCalledWith({
        img: 'img/arangodb_logo_small.png',
        name: '',
        username: 'userName',
        active: true
      });
    });

    it('should render with image', function () {
      view.template = {
        render: function () {}
      };

      var userDummy = {
        set: function () {},
        get: function () {}
      };

      spyOn(collectionDummy, 'whoAmI').andReturn('userName');
      spyOn(collectionDummy, 'findWhere').andReturn(userDummy);
      spyOn(userDummy, 'set');
      spyOn(userDummy, 'get').andCallFake(function (a) {
        if (a === 'active') {
          return true;
        }
        return {
          img: 'bla',
          name: 'name'
        };
      });

      spyOn(view.template, 'render').andReturn(1);

      view.render();

      expect(userDummy.set).toHaveBeenCalledWith({loggedIn: true});
      expect(userDummy.get).toHaveBeenCalledWith('extra');
      expect(userDummy.get).toHaveBeenCalledWith('active');
      expect(collectionDummy.whoAmI).toHaveBeenCalled();
      expect(collectionDummy.findWhere).toHaveBeenCalledWith({user: 'userName'});
      expect(view.template.render).toHaveBeenCalledWith({
        img: 'https://s.gravatar.com/avatar/bla?s=24',
        name: 'name',
        username: 'userName',
        active: true
      });
    });
  });
}());
