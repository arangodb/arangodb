/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global runs, waitsFor, jasmine*/
/* global $, arangoCollectionModel*/
(function () {
  'use strict';

  describe('Plan Symmetric/Asymmetric View', function () {
    var myView, ip_base, ajaxFake;

    beforeEach(function () {
      $('body').append('<div id="content" class="removeMe"></div>');

      ajaxFake = {
        done: function () {
          return undefined;
        }
      };
      myView = new window.PlanSymmetricView();
      myView.render(true);

      ip_base = '123.456.78';

      spyOn($, 'ajax').andReturn(ajaxFake);
      spyOn(window, 'alert');
    });

    afterEach(function () {
      $('.removeMe').remove();
    });

    it('should start a symmetrical cluster', function () {
      myView.render(true);
      $('.add').click();
      $('.add').click();
      $('.dispatcher').each(function (i, d) {
        $('.host', d).val(ip_base + i);
        $('.port', d).val(i);
      });
      $('#startPlan').click();
      expect($.ajax).toHaveBeenCalledWith('cluster/plan', {
        type: 'POST',
        data: JSON.stringify({
          dispatcher: [
            {host: ip_base + '0:0'},
            {host: ip_base + '1:1'},
            {host: ip_base + '2:2'}
          ],
          type: 'symmetricalSetup'
        })
      });
    });
    it('should start a asymmetrical cluster', function () {
      myView.render(false);
      $('.add').click();
      $('.add').click();
      $('.dispatcher').each(function (i, d) {
        $('.host', d).val(ip_base + i);
        $('.port', d).val(i);
        if (i === 0) {
          $('.isDBServer', d).attr('checked', true);
          $('.isCoordinator', d).attr('checked', false);
        } else if (i === 1) {
          $('.isDBServer', d).attr('checked', false);
          $('.isCoordinator', d).attr('checked', true);
        } else {
          $('.isDBServer', d).attr('checked', true);
          $('.isCoordinator', d).attr('checked', true);
        }
      });

      $('#startPlan').click();
      expect($.ajax).toHaveBeenCalledWith('cluster/plan', {
        type: 'POST',
        data: JSON.stringify({
          dispatcher: [
            {host: ip_base + '0:0', isDBServer: true, isCoordinator: false},
            {host: ip_base + '1:1', isDBServer: false, isCoordinator: true},
            {host: ip_base + '2:2', isDBServer: true, isCoordinator: true}
          ],
          type: 'asymmetricalSetup'
        })
      });
    });

    it('should not start a symmetrical cluster because no complete host ist provided', function () {
      myView.render(true);
      $('.add').click();
      $('.add').click();
      $('.dispatcher').each(function (i, d) {
        if (i === 0) {
          $('.host', d).val(ip_base + i);
        } else {
          $('.port', d).val(i);
        }
      });
      $('#startPlan').click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith('Please provide at least one Host');
    });

    it('should not start an asymmetrical cluster without dbserver and coordinator', function () {
      myView.render(false);
      $('.add').click();
      $('.add').click();
      $('.dispatcher').each(function (i, d) {
        $('.host', d).val(ip_base + i);
        $('.port', d).val(i);
        if (i === 0) {
          $('.isDBServer', d).attr('checked', false);
          $('.isCoordinator', d).val(false);
        } else if (i === 1) {
          $('.isDBServer', d).attr('checked', false);
          $('.isCoordinator', d).attr('checked', false);
        } else {
          $('.isDBServer', d).attr('checked', false);
          $('.isCoordinator', d).attr('checked', false);
        }
      });
      $('#startPlan').click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith('Please provide at least one DBServer');
    });
    it('should not start an asymmetrical cluster because no coordinator ist provided', function () {
      myView.render(false);
      $('.add').click();
      $('.add').click();
      $('.dispatcher').each(function (i, d) {
        $('.host', d).val(ip_base + i);
        $('.port', d).val(i);
        if (i === 0) {
          $('.isDBServer', d).attr('checked', true);
          $('.isCoordinator', d).attr('checked', false);
        } else if (i === 1) {
          $('.isDBServer', d).attr('checked', false);
          $('.isCoordinator', d).attr('checked', false);
        } else {
          $('.isDBServer', d).attr('checked', false);
          $('.isCoordinator', d).attr('checked', false);
        }
      });
      $('#startPlan').click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith('Please provide at least one Coordinator');
    });

    it('should be able to add  3 and remove 2 input lines for hosts', function () {
      myView.render(false);
      $('.add').click();
      $('.add').click();
      $('.add').click();
      $('.dispatcher').each(function (i, d) {
        if (i >= 2) {
          $('.delete', d).click();
        }
      });
      expect($('.dispatcher').length).toEqual(2);
    });
  });
}());
