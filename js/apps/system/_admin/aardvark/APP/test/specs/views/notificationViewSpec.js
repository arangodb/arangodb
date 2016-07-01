/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('The notification view', function () {
    var view, div, div2, fakeNotification, dummyCollection, jQueryDummy;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'navigationBar';
      div2 = document.createElement('div');
      div2.id = 'notificationBar';
      document.body.appendChild(div);
      document.body.appendChild(div2);

      dummyCollection = new window.NotificationCollection();

      fakeNotification = {
        title: 'Hallo',
        date: 1398239658,
        content: '',
        priority: '',
        tags: '',
        seen: false
      };

      view = new window.NotificationView({
        collection: dummyCollection
      });

      view.render();
    });

    afterEach(function () {
      document.body.removeChild(div);
      document.body.removeChild(div2);
    });

    it('assert basics', function () {
      expect(view.events).toEqual({
        'click .navlogo #stat_hd': 'toggleNotification',
        'click .notificationItem .fa': 'removeNotification',
        'click #removeAllNotifications': 'removeAllNotifications'
      });
    });

    it('toggleNotification should run function', function () {
      jQueryDummy = {
        toggle: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(jQueryDummy, 'toggle');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.toggleNotification();
      expect(jQueryDummy.toggle).not.toHaveBeenCalled();
    });

    it('toggleNotification should run function', function () {
      view.collection.add(fakeNotification);
      jQueryDummy = {
        toggle: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(jQueryDummy, 'toggle');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.toggleNotification();
      expect(jQueryDummy.toggle).toHaveBeenCalled();
    });

    it('toggleNotification should run function', function () {
      spyOn(view.collection, 'reset');
      view.removeAllNotifications();
      expect(view.collection.reset).toHaveBeenCalled();
      expect($('#notification_menu').is(':visible')).toBeFalsy();
    });

    it('renderNotifications function with collection length 0', function () {
      jQueryDummy = {
        html: function () {
          throw 'Should be a spy';
        },
        text: function () {
          throw 'Should be a spy';
        },
        removeClass: function () {
          throw 'Should be a spy';
        },
        hide: function () {
          throw 'Should be a spy';
        }
      };

      spyOn(jQueryDummy, 'html');
      spyOn(jQueryDummy, 'text');
      spyOn(jQueryDummy, 'removeClass');
      spyOn(jQueryDummy, 'hide');

      spyOn(window, '$').andReturn(
        jQueryDummy
      );

      view.renderNotifications();
      expect(jQueryDummy.text).toHaveBeenCalled();
      expect(jQueryDummy.removeClass).toHaveBeenCalled();
      expect(jQueryDummy.hide).toHaveBeenCalled();
      expect(jQueryDummy.html).toHaveBeenCalled();
    });

    it('renderNotifications function with collection length > 0', function () {
      view.collection.add(fakeNotification);

      jQueryDummy = {
        html: function () {
          throw 'Should be a spy';
        },
        text: function () {
          throw 'Should be a spy';
        },
        addClass: function () {
          throw 'Should be a spy';
        }
      };

      spyOn(jQueryDummy, 'html');
      spyOn(jQueryDummy, 'text');
      spyOn(jQueryDummy, 'addClass');

      spyOn(window, '$').andReturn(
        jQueryDummy
      );

      view.renderNotifications();
      expect(jQueryDummy.text).toHaveBeenCalled();
      expect(jQueryDummy.addClass).toHaveBeenCalled();
      expect(jQueryDummy.html).toHaveBeenCalled();
    });

    it('remove a single notification', function () {
      view.render();

      view.collection.add(fakeNotification);
      view.collection.add(fakeNotification);
      view.collection.add(fakeNotification);

      var firstModel = view.collection.first(),
        getDummy = {
          destroy: function () {
            throw 'Should be a spy';
          }
      };

      spyOn(getDummy, 'destroy');
      spyOn(view.collection, 'get').andReturn(
        getDummy
      );

      $('#stat_hd').click();
      $('#' + firstModel.cid).click();

      expect(getDummy.destroy).toHaveBeenCalled();
    });
  });
}());
