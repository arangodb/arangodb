/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global runs, waitsFor, jasmine*/
/* global $, arangoCollectionModel*/
(function () {
  'use strict';

  describe('Plan Test View', function () {
    var myView, ip, port, coords, dbs, ajaxFake;

    beforeEach(function () {
      $('body').append('<div id="content" class="removeMe"></div>');

      ip = '192.168.0.1';
      port = 8529;
      coords = 4;
      dbs = 5;
      ajaxFake = {
        done: function () {}
      };

      myView = new window.PlanTestView();
      myView.render();

      $('#host').val(ip);
      $('#port').val(port);
      $('#coordinators').val(coords);
      $('#dbs').val(dbs);
      spyOn($, 'ajax').andReturn(ajaxFake);
      spyOn(window, 'alert');
    });

    afterEach(function () {
      $('.removeMe').remove();
    });

    it('should start a cluster', function () {
      $('#startPlan').click();
      expect($.ajax).toHaveBeenCalledWith('cluster/plan', {
        type: 'POST',
        data: JSON.stringify({
          type: 'testSetup',
          dispatcher: ip + ':' + port,
          numberDBServers: dbs,
          numberCoordinators: coords
        })
      });
    });
    it('should not start a cluster if dispatcher port is missing', function () {
      $('#host').val('');
      $('#startPlan').click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith('Please define a Host');
    });

    it('should not start a cluster if dispatcher host is missing', function () {
      $('#port').val('');
      $('#startPlan').click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith('Please define a Port');
    });

    it('should not start a cluster if coordinator number is missing', function () {
      $('#coordinators').val('');
      $('#startPlan').click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith('Please define a number of Coordinators');
    });

    it('should not start a cluster if db number is missing', function () {
      $('#dbs').val('');
      $('#startPlan').click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith('Please define a number of DBServers');
    });
  });
}());
