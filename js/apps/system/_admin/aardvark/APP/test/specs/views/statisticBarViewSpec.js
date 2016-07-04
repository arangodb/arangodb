/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/* global window, document, hljs, $*/

(function () {
  'use strict';

  describe('The StatisticBarView', function () {
    var view;

    beforeEach(function () {
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        }
      };

      view = new window.StatisticBarView(
        {
          currentDB: {
            get: function () {
              return true;
            }
          }
        });
      expect(view.events).toEqual({
        'change #arangoCollectionSelect': 'navigateBySelect',
        'click .tab': 'navigateByTab'
      });
    });

    afterEach(function () {
      delete window.App;
    });

    it('should render', function () {
      view.template = {
        render: function () {}
      };

      spyOn(view.template, 'render').andReturn(1);
      spyOn(view, 'replaceSVG');
      spyOn($.fn, 'html');
      spyOn($.fn, 'find').andCallThrough();
      spyOn($.fn, 'attr').andCallThrough();
      spyOn($.fn, 'each').andCallFake(function (a) {
        a();
      });
      spyOn($, 'get').andCallFake(function (a, b, c) {
        b();
      });

      view.render();

      expect($.fn.html).toHaveBeenCalledWith(1);
      expect($.fn.find).toHaveBeenCalledWith('img.svg');
      expect(view.replaceSVG).toHaveBeenCalled();
      /*
      expect($.fn.attr).toHaveBeenCalledWith('id')
      expect($.fn.attr).toHaveBeenCalledWith('class')
      expect($.fn.attr).toHaveBeenCalledWith('src')
      */
      expect(view.template.render).toHaveBeenCalledWith({
        isSystem: true
      });
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
      };

      spyOn(window.App, 'navigate');
      spyOn(p, 'preventDefault');
      view.navigateByTab(p);
      expect(p.preventDefault).toHaveBeenCalled();
      expect(window.App.navigate).toHaveBeenCalledWith('bla', {trigger: true});
    });

    it('should navigateByTab to links', function () {
      var p = {
        srcElement: {id: 'links'},
        preventDefault: function () {}
      };

      spyOn(window.App, 'navigate');
      spyOn(p, 'preventDefault');
      view.navigateByTab(p);
      expect(p.preventDefault).toHaveBeenCalled();
      expect(window.App.navigate).not.toHaveBeenCalled();
    });

    it('should navigateByTab to tools', function () {
      var p = {
        srcElement: {id: 'tools'},
        preventDefault: function () {}
      };

      spyOn(p, 'preventDefault');
      view.navigateByTab(p);
      expect(p.preventDefault).toHaveBeenCalled();
    });

    it('should handleSelectNavigation', function () {
      var jQueryDummy = {
        change: function () {},
        find: function () {
          return jQueryDummy;
        },
        val: function () {}
      };

      spyOn(window, '$').andReturn(jQueryDummy);
      spyOn(window.App, 'navigate');
      spyOn(jQueryDummy, 'val').andReturn('bla');
      spyOn(jQueryDummy, 'find').andCallThrough();
      spyOn(jQueryDummy, 'change').andCallFake(function (a) {
        a();
      });
      view.handleSelectNavigation();
      expect(window.$).toHaveBeenCalledWith('#arangoCollectionSelect');
      expect(jQueryDummy.find).toHaveBeenCalledWith('option:selected');
      expect(jQueryDummy.change).toHaveBeenCalled();
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(window.App.navigate).toHaveBeenCalledWith('bla', {trigger: true});
    });

    it('should selectMenuItem', function () {
      var jQueryDummy = {
        removeClass: function () {},
        addClass: function () {}
      };

      spyOn(window, '$').andReturn(jQueryDummy);
      spyOn(jQueryDummy, 'removeClass');
      spyOn(jQueryDummy, 'addClass');
      view.selectMenuItem('mm');
      expect(window.$).toHaveBeenCalledWith('.navlist li');
      expect(window.$).toHaveBeenCalledWith('.mm');
      expect(jQueryDummy.removeClass).toHaveBeenCalledWith('active');
      expect(jQueryDummy.addClass).toHaveBeenCalledWith('active');
    });
  });
}());
